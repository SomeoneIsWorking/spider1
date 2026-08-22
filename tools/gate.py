#!/usr/bin/env python3
"""gate.py — the AGENT's headless RUN GATE for this port. Never `./run.sh`.

WHY THIS EXISTS. Until now NOTHING in this repo drove the built binary (docs/issues/0014): every
dynamic claim was measured by hand-launching `spiderman_port` with env vars, so nothing mechanical
would have noticed if boot regressed. `./run.sh` is the USER's windowed play launcher — it resolves
the framework checkout, syncs nested dependencies, re-extracts, rebuilds and opens a window — so an
agent must not use it for a bounded measurement. This tool does none of that: it drives an
ALREADY-BUILT binary.

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target spiderman_port -j$(nproc)

WHY IT IS NOT A COPY OF Tomba2Engine/tools/gate.py. That gate drives the game over the framework
REPL. THIS PORT NEVER ENTERS THE FRAMEWORK FRAME LOOP THAT SERVICES THE REPL: game/core/game_hooks.cpp
rec_dispatches the guest main, which never returns, and its frameUpdate/drawOTag hooks are deliberate
abort() fail-fasts. `PSXPORT_REPL=1` is reported by the port's own cfg audit as an UNKNOWN knob that
"did NOTHING in this run". So this gate cannot step the game; it launches it capped and keys on the
PORT'S OWN LOG LINES. Every assertion below quotes the line it reads.

WHAT A PASS MEANS, STATED SO IT CANNOT BE OVERREAD. A green gate means: the binary launched headless,
loaded the boot executable, dispatched the guest main on the substrate, the render seam installed AND
FIRED, the frame/submit counters ADVANCED at a rate near the recorded baseline, the run got past the
boot-init scene '....' into named scenes, and no failure pattern appeared. It says NOTHING about
pixels, about the native render leg (the default leg is psx_render, the reference), or about anything
past the ~2 minutes it ran.

REFUSALS (exit 2, never 0) — each says what it did NOT do rather than returning a clean empty pass:
missing binary, missing boot executable, unresolvable disc, gpuguard missing/latched or refusing
preflight, zero output lines, no port output at all (launcher never got there), fewer than two
progress lines to compare (cannot speak about ADVANCE), and a HANG the run's own two kill paths failed
to end — which kills the whole process GROUP (the port is a grandchild of gpuguard, and killing only
the direct child leaks a GPU-holding orphan into the next run) and reports how many processes it
signalled, how many survived, and how many output lines it captured before the kill.
GPU DEVICE LOSS (exit 3): a hard STOP for the whole session, not a retry.

    python3 tools/gate.py boot                     # the gate: capped headless launch + assertions
    python3 tools/gate.py boot --seconds 240       # longer run; the floors scale with duration
    python3 tools/gate.py check-log <path>         # re-run the SAME analyser over a captured log
    python3 tools/gate.py --selftest               # prove the gate FIRES: exits 0 only if the
                                                   # known-good capture passed AND every broken
                                                   # variant was caught (it prints each verdict)
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

from disc_path import resolve_disc as resolve_disc_path

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = os.path.join(REPO, 'scratch', 'bin', 'spiderman_port')
EXE = os.path.join(REPO, 'scratch', 'bin', 'spiderman', 'SLUS_008.75')
LOGDIR = os.path.join(REPO, 'scratch', 'logs')

# ---- THE RECORDED BASELINE ---------------------------------------------------------------------
# Measured 2026-08-12 at psxport 240d8f9a (docs/issues/0014): a 240s headless run reached rseam frame
# 14007 with 5632 submitFrame calls and 9 scene changes, clean.
#
# The floors below are RATES times the duration, deliberately NOT an absolute end frame: an absolute
# frame is a hardcoded expected value that fails for reasons unrelated to a regression (a longer boot,
# a shorter cap, a busier machine). What is asserted is that the run ADVANCED at a rate in the same
# league as the baseline.
BASE_SECS = 240
BASE_FRAMES = 14007
BASE_SUBMITS = 5632
# LOAD SENSITIVITY, stated rather than hidden: several agents share this machine, so wall-clock rate is
# not a pure function of the code. 0.35 is a third of baseline throughput — low enough that ordinary
# contention passes, high enough that a boot which stalls or crawls fails. A failure on this assertion
# alone, with every other assertion green, is the one result to re-run before believing.
RATE_MARGIN = 0.35
# Scene changes are event-counted, not rate-counted: reaching a NAMED scene at all is the qualitative
# step (the boot-init scene is '....', unprintable). Baseline saw 9 in 240s; 2 is the floor that says
# "left the boot-init scene and moved on again".
MIN_SCENE_CHANGES = 2

# ---- the log lines this gate keys on -----------------------------------------------------------
# Each regex is quoted from a real measured run (scratch/logs/probe-boot400.log).
# "[boot] loaded scratch/bin/spiderman/SLUS_008.75: entry 0x8008739C load 0x80010000 text 0xB6800 ..."
RE_BOOT_LOADED = re.compile(r'\[boot\] loaded \S+: entry 0x([0-9A-Fa-f]+)')
# "[boot] Phase 0: dispatching guest main() 0x8002C354 on the recompiled substrate"
RE_GUEST_MAIN = re.compile(r'\[boot\] Phase 0: dispatching guest main\(\) 0x([0-9A-Fa-f]+)')
# "[rseam] render seam installed at 0x80061308 (the engine's submitFrame ...)"
RE_SEAM_INSTALLED = re.compile(r'\[rseam\] render seam installed at 0x([0-9A-Fa-f]+)')
# "[rseam] submitFrame override REACHED — call #1 at frame 2, ra=80061218, leg=psx_render ..."
# The port's own install line says: "If no '[rseam] submitFrame override REACHED' line follows, it
# never ran." That makes this the port's own designated proof-of-fire, so the gate uses it as one.
RE_SEAM_REACHED = re.compile(r'\[rseam\] submitFrame override REACHED\b.*?at frame (\d+)')
# "[rseam] submitFrame calls=5632 frame=14007 leg=psx scene='l1a1' code=0x0101 sceneChanges=9"
RE_PROGRESS = re.compile(r"\[rseam\] submitFrame calls=(\d+) frame=(\d+) leg=(\S+) scene='([^']*)' "
                         r"code=0x([0-9A-Fa-f]+) sceneChanges=(\d+)")
# "[scene] CHANGED scene identity: name='l1a1' raw=6C,31,61,31 code=0x0101 printable=1 (call #..., frame ...)"
RE_SCENE = re.compile(r"\[scene\] (FIRST|CHANGED) scene identity: name='([^']*)'.*?printable=(\d)")
# "[cfg:warn] UNKNOWN knob PSXPORT_REPL is set and matched nothing — it did NOTHING in this run"
RE_UNKNOWN_KNOB = re.compile(r'UNKNOWN knob (\S+)')
# The port's own render-leg stamp, reported so a run is never mistaken for the other leg.
RE_RENDER_PATH = re.compile(r'\[render\] render path = (\S+)')

# Failure patterns. CALIBRATED against the known-good baseline log rather than guessed: a plain
# /\babort\b/ (which is what the sibling gate uses) MATCHES the port's healthy startup banner —
# "[rseam] default render leg = psx_render ... then aborts at the next scene naming it. That abort is
# the correct result" — so it would have failed every green run. Patterns here are anchored to how a
# failure actually PRINTS: lucent renders an error as "[<channel>:error]", so the seam's fail-fast is
# "[FATAL:error]". Likewise /STUCK/ is case-sensitive and tag-anchored, because the ordinary
# timeout-kill line contains the word "stuck" in prose ("where it was stuck").
FAIL_PATTERNS = [
    r'\[FATAL:error\]',
    r'unimplemented native rendering',
    r'recomp[- ]MISS',
    r'rec_dispatch miss',
    r'\[watchdog\] STUCK',
    r'Fps60::rq_capture OVERFLOW',
    r'Segmentation fault',
    r'std::bad_alloc',
    r'display_pass_write_guard|guest write during the display pass',
    r'terminate called',
]
# A GPU fault is not a failing assertion — it is the end of GPU work for the session (CLAUDE.md).
DEVICE_LOSS_PATTERNS = [
    r'VK_ERROR_DEVICE_LOST',
    r'context is lost',
    r'VRAM is lost due to GPU reset',
    r'GPU reset',
]
# The ordinary, EXPECTED end of a healthy capped run: the port's watchdog handler catches the
# supervisor's SIGTERM, prints where it was, and _exit(130)s. That is not a failure.
RE_INTERRUPT = re.compile(r'\[watchdog\] INTERRUPT \(SIGINT/SIGTERM\)')


def refuse(msg: str) -> int:
    print(f"GATE REFUSED: {msg}", file=sys.stderr)
    return 2


def _group_members(pgid: int) -> list[str]:
    """Every live process in PGID, as 'pid args' rows. Used to COUNT what a kill actually reached —
    'killed the group' with no denominator is the kind of claim this repo does not accept."""
    ps = subprocess.run(['ps', '-eo', 'pid,pgid,args'], capture_output=True, text=True)
    rows = []
    for line in ps.stdout.splitlines()[1:]:
        parts = line.split(None, 2)
        if len(parts) == 3 and parts[1].isdigit() and int(parts[1]) == pgid and int(parts[0]) != os.getpid():
            rows.append(f"{parts[0]} {parts[2]}")
    return rows


def _kill_group(pid: int) -> tuple[int, int]:
    """SIGKILL the whole process group (never by binary NAME — siblings run the same binaries, so
    `pkill -f spiderman_port` would kill another agent's run mid-gate). Returns
    (processes signalled, processes still alive after)."""
    try:
        pgid = os.getpgid(pid)
    except ProcessLookupError:
        return (0, 0)
    if pgid == os.getpgid(0):        # refuse to kill our OWN group — that would kill this gate
        print(f"[gate] REFUSING to kill process group {pgid}: it is this process's own group.",
              file=sys.stderr)
        return (0, len(_group_members(pgid)))
    members = _group_members(pgid)
    try:
        os.killpg(pgid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    time.sleep(1.0)
    return (len(members), len(_group_members(pgid)))


def resolve_disc() -> str | None:
    """Return a valid disc selected by the shared launcher/provisioning policy."""
    d = resolve_disc_path(Path(REPO), None, os.environ)
    return d if d and os.path.isfile(d) else None


# ------------------------------------------------------------------------------------------------
# THE ANALYSER. Pure function of (text, exit code, seconds) so --selftest can feed it captured and
# mutated logs. A gate whose judgement can only be exercised by launching the game is a gate nobody
# can prove fires.
# ------------------------------------------------------------------------------------------------
def analyse(out: str, rc: int | None, secs: float, label: str, log_hint: str = '') -> int:
    lines = out.count('\n')
    if lines == 0:
        return refuse(f"ZERO output lines in {secs:.1f}s (exit {rc}). Nothing was observed, so nothing "
                      f"is proven. {log_hint}")

    boot = RE_BOOT_LOADED.search(out)
    gmain = RE_GUEST_MAIN.search(out)
    seam_inst = RE_SEAM_INSTALLED.search(out)
    seam_fire = RE_SEAM_REACHED.search(out)
    prog = RE_PROGRESS.findall(out)
    scenes = RE_SCENE.findall(out)
    leg = RE_RENDER_PATH.search(out)
    unknown = sorted(set(RE_UNKNOWN_KNOB.findall(out)))

    # Frames come from BOTH the periodic [rseam] line and the [scene] lines' "(call #N, frame M)", so a
    # run that changed scene after its last periodic line still reports its true reach.
    frames = [int(f) for (_c, f, _l, _s, _k, _sc) in prog]
    frames += [int(m) for m in re.findall(r'\(call #\d+, frame (\d+)\)', out)]
    calls = [int(c) for (c, _f, _l, _s, _k, _sc) in prog]
    if seam_fire:
        frames.append(int(seam_fire.group(1)))
    sc_counts = [int(sc) for (_c, _f, _l, _s, _k, sc) in prog]
    scene_changes = max(sc_counts) if sc_counts else sum(1 for k, _n, _p in scenes if k == 'CHANGED')
    named = sorted({n for _k, n, p in scenes if p == '1'})

    maxframe = max(frames) if frames else -1
    maxcalls = max(calls) if calls else -1
    dev = [(p, m.group(0)) for p in DEVICE_LOSS_PATTERNS for m in [re.search(p, out)] if m]

    hits = []
    for pat in FAIL_PATTERNS:
        m = re.search(pat, out)
        if m:
            ctx = out[max(0, m.start() - 120):m.end() + 200].replace('\n', ' | ')
            hits.append((pat, ctx))

    min_frames = int(BASE_FRAMES / BASE_SECS * secs * RATE_MARGIN)
    min_calls = int(BASE_SUBMITS / BASE_SECS * secs * RATE_MARGIN)

    # ---- the report, ALWAYS with its denominator ------------------------------------------------
    print(f"[gate:{label}] exit={rc} in {secs:.1f}s · {lines} output line(s) · "
          f"{len(prog)} periodic [rseam] progress line(s) · {len(scenes)} [scene] identity line(s)")
    print(f"[gate:{label}] scanned {lines} line(s) against {len(FAIL_PATTERNS)} failure pattern(s) "
          f"and {len(DEVICE_LOSS_PATTERNS)} GPU-device-loss pattern(s)")
    print(f"[gate:{label}] boot exe entry={('0x' + boot.group(1)) if boot else 'NOT SEEN'} · "
          f"guest main={('0x' + gmain.group(1)) if gmain else 'NOT SEEN'} · "
          f"seam installed at={('0x' + seam_inst.group(1)) if seam_inst else 'NOT SEEN'} · "
          f"seam FIRED at frame={seam_fire.group(1) if seam_fire else 'NEVER'}")
    print(f"[gate:{label}] reach: frame {maxframe if maxframe >= 0 else 'NONE'} · "
          f"submitFrame calls {maxcalls if maxcalls >= 0 else 'NONE'} · scene changes {scene_changes} · "
          f"named scenes {', '.join(named) if named else '(none — never left the boot-init scene)'}")
    print(f"[gate:{label}] floors for {secs:.0f}s at {RATE_MARGIN:.0%} of the recorded baseline "
          f"({BASE_FRAMES} frames / {BASE_SUBMITS} submits in {BASE_SECS}s): "
          f"frame >= {min_frames}, calls >= {min_calls}, scene changes >= {MIN_SCENE_CHANGES}")
    if leg:
        print(f"[gate:{label}] render leg this run: {leg.group(1)} (the default is psx_render, the "
              f"reference; pc_render has no display-list producer and aborts by design)")
    if unknown:
        print(f"[gate:{label}] note: {len(unknown)} knob(s) the port reported UNKNOWN at startup: "
              f"{', '.join(unknown)}. Not gated on — the startup audit runs before late subsystems "
              f"register, and PSXPORT_SPIDERMAN_DISC is resolved outside the CVar registry (the [cd] "
              f"pump line proves the media was read).")
    if log_hint:
        print(f"[gate:{label}] {log_hint}")

    # ---- device loss FIRST: it outranks every assertion ----------------------------------------
    if dev:
        print(f"[gate:{label}] GPU DEVICE LOSS — {len(dev)} pattern(s) matched. STOP all GPU runs for "
              f"this session and report it; do NOT retry (CLAUDE.md). Matches:")
        for pat, txt in dev:
            print(f"    /{pat}/  {txt}")
        return 3

    bad = 0

    def fail(msg: str) -> None:
        nonlocal bad
        print(f"[gate:{label}] FAIL — {msg}")
        bad = 1

    if not boot:
        fail("no '[boot] loaded …: entry 0x…' line — the boot executable was never loaded, so the "
             "guest never started. This is not a pass.")
    if not gmain:
        fail("no '[boot] Phase 0: dispatching guest main()' line — the substrate was never entered.")
    if not seam_inst:
        fail("no '[rseam] render seam installed at 0x…' line — the render seam was NOT wired, so this "
             "run's picture is entirely the guest's and the port's one native producer is absent.")
    if not seam_fire:
        fail("no '[rseam] submitFrame override REACHED' line — the seam installed but NEVER RAN. The "
             "port's own install line names this exact line as the proof it fired.")
    if maxframe < 0:
        fail("no frame counter ever appeared, so the run cannot be said to have advanced.")
    if hits:
        print(f"[gate:{label}] FAIL — {len(hits)} failure pattern(s) matched:")
        for pat, ctx in hits:
            print(f"    /{pat}/  …{ctx}…")
        bad = 1

    # ORDER MATTERS HERE, and getting it wrong is how a gate turns a real failure into "nothing
    # proven". The short-log REFUSAL must come AFTER the pattern and missing-line checks: measured on a
    # real captured log (scratch/re20/logs/pcleg_final.log — the pc_render leg's unimplemented-scene
    # abort at submitFrame call #2), an earlier version of this function refused with exit 2 over a log
    # whose FATAL line it had already read, because the run was too short to have two progress lines.
    # An abort is a FAILURE regardless of how far it got; only an otherwise-CLEAN short run is a refusal.
    if len(prog) < 2:
        if bad:
            print(f"[gate:{label}] (only {len(prog)} periodic progress line(s), so nothing is asserted "
                  f"about ADVANCE — but the failure above stands on its own.)")
            return 1
        return refuse(f"only {len(prog)} periodic [rseam] progress line(s) (the port prints one every "
                      f"512 submitFrame calls) and NO failure pattern matched. Two lines are needed to "
                      f"speak about ADVANCE at all — run longer with --seconds. Nothing about progress "
                      f"is proven. {log_hint}")
    first_frame, last_frame = int(prog[0][1]), int(prog[-1][1])
    first_calls, last_calls = int(prog[0][0]), int(prog[-1][0])
    if last_frame <= first_frame or last_calls <= first_calls:
        fail(f"the counters did NOT advance between the first and last progress line "
             f"(frame {first_frame} -> {last_frame}, calls {first_calls} -> {last_calls}).")
    else:
        print(f"[gate:{label}] advance: frame {first_frame} -> {last_frame} "
              f"(+{last_frame - first_frame}), calls {first_calls} -> {last_calls} "
              f"(+{last_calls - first_calls})")
    if maxframe >= 0 and maxframe < min_frames:
        fail(f"reached frame {maxframe} in {secs:.0f}s, floor {min_frames}. The run advanced far too "
             f"slowly against the recorded baseline. NOTE this assertion is load-sensitive — if it is "
             f"the ONLY failure, re-run before believing it.")
    if maxcalls >= 0 and maxcalls < min_calls:
        fail(f"only {maxcalls} submitFrame call(s) in {secs:.0f}s, floor {min_calls}. Same load "
             f"caveat as the frame floor.")
    if scene_changes < MIN_SCENE_CHANGES:
        fail(f"{scene_changes} scene change(s), floor {MIN_SCENE_CHANGES} — the run did not get past "
             f"the boot-init scene into named scenes and back. Named scenes seen: "
             f"{', '.join(named) if named else '(none)'}.")
    # Exit code. A healthy capped run is KILLED by the supervisor: the port's watchdog catches SIGTERM
    # and _exit(130)s, which is the normal end here and must not read as a failure. Anything else
    # non-zero without a matched pattern means the pattern list is incomplete for that failure.
    interrupted = bool(RE_INTERRUPT.search(out))
    if rc is not None and rc not in (0, 130) and not hits:
        fail(f"exit {rc} with no failure pattern matched"
             + (" (SIGABRT — the port fail-fasted; read the log, a pattern is missing)"
                if rc == 134 else "")
             + f". Read the log: the pattern list is incomplete for this failure.")
    elif rc == 130 and not interrupted:
        fail("exit 130 but no '[watchdog] INTERRUPT' line — the process died on a signal without the "
             "port's own handler running, which is not the ordinary capped-run ending.")

    if bad:
        return 1
    print(f"[gate:{label}] PASS — launched headless, loaded the boot exe, dispatched the guest main, "
          f"seam installed AND fired, advanced to frame {maxframe} / {maxcalls} submitFrame calls / "
          f"{scene_changes} scene changes ({', '.join(named) if named else 'no named scene'}), "
          f"no failure pattern matched. Says NOTHING about pixels, about the pc_render leg, or about "
          f"anything past the {secs:.0f}s it ran.")
    return 0


# ------------------------------------------------------------------------------------------------
# the launcher
# ------------------------------------------------------------------------------------------------
def cmd_boot(args) -> int:
    if not os.path.isfile(BIN):
        return refuse(f"{BIN} does not exist — NOTHING WAS RUN. Build first: cmake --build build "
                      f"--target spiderman_port -j$(nproc). Do not read this as a pass.")
    if not os.path.isfile(EXE):
        return refuse(f"{EXE} does not exist — NOTHING WAS RUN. The boot executable is extracted from "
                      f"the disc by tools/ensure_recomp.py (run.sh's job, and run.sh is the user's). "
                      f"Provision it once, then re-run this gate.")
    disc = resolve_disc()
    if not disc:
        return refuse("no disc image resolved (PSXPORT_SPIDERMAN_DISC, .env, or a *.chd here) — "
                      "NOTHING WAS RUN. Without media the [cd] pump serves nothing and the run would "
                      "not be comparable to the recorded baseline.")

    cmd = [BIN, EXE]
    launcher = 'direct'
    if not args.no_gpuguard:
        gg = shutil.which('gpuguard')
        if not gg:
            return refuse("gpuguard is not on PATH — NOTHING WAS RUN. This run drives a Vulkan "
                          "device, and the machine-wide GPU interlock is not optional here "
                          "(CLAUDE.md). On a machine without it, pass --no-gpuguard deliberately.")
        cmd = [gg, 'run', '--timeout', str(args.seconds), '--'] + cmd
        launcher = 'gpuguard'

    env = dict(os.environ)
    env['PSXPORT_SPIDERMAN_DISC'] = disc
    env['PSXPORT_NOAUDIO'] = '1'
    env['PSXPORT_NOPACE'] = '1'          # the ONLY switch that means "frames, not real time"
    env['PSXPORT_WATCHDOG'] = str(args.watchdog)
    # Assets live with the FRAMEWORK; without this the overlay logs four [rmlui:error] lines about
    # missing fonts on every otherwise-clean run. Not a knob the gate asserts on — just noise removed.
    env.setdefault('PSXPORT_ASSET_DIR', os.path.join(REPO, 'external', 'psxport'))
    # NOT setting PSXPORT_VK_HEADLESS: the binary is headless BY DEFAULT (run.sh opts INTO a window
    # with PSXPORT_VK_WINDOW=1), precisely so an agent that forgets a flag fails safe.
    if args.debug:
        env['PSXPORT_DEBUG'] = args.debug

    os.makedirs(LOGDIR, exist_ok=True)
    stamp = time.strftime('%Y%m%d-%H%M%S')
    logpath = os.path.join(LOGDIR, f'gate-boot-{stamp}.log')

    print(f"[gate:boot] launching via {launcher}, capped at {args.seconds}s, "
          f"PSXPORT_WATCHDOG={args.watchdog}s frame-progress")
    print(f"[gate:boot] {' '.join(cmd)}")
    # PROCESS GROUP, not a bare child, and this is MEASURED rather than assumed: `subprocess.run(...,
    # timeout=)` kills only the DIRECT child, so killing the gpuguard wrapper leaves `spiderman_port`
    # alive and reparented to pid 1 — verified 2026-08-12 with a `gpuguard run -- sh -c 'sleep 200'`
    # stand-in, whose sleeper survived the wrapper's death. An orphan holding the Vulkan device is
    # exactly what the next gate run then contends with, while this run reports a tidy refusal. So the
    # launch gets its own session and the hang path kills the whole GROUP and COUNTS the survivors.
    grace = args.grace
    t0 = time.time()
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env,
                         cwd=REPO, stdin=subprocess.DEVNULL, start_new_session=True)
    try:
        so, se = p.communicate(timeout=args.seconds + grace)
        rc = p.returncode
    except subprocess.TimeoutExpired:
        killed, alive = _kill_group(p.pid)
        try:
            so, se = p.communicate(timeout=30)
        except subprocess.TimeoutExpired:
            so, se = '', ''
        # BOTH streams: the port writes 100% of its output to STDERR (measured 2026-08-12 — 92 lines on
        # stderr, 0 on stdout), so an earlier version of this handler, which saved only stdout, wrote an
        # EMPTY log for the single failure that needs a log most. A refusal pointing at an empty file is
        # the "diagnostic that can print nothing" this repo's rules forbid.
        out = (so or '') + (se or '')
        with open(logpath, 'w') as f:
            f.write(out)
        return refuse(f"neither the {args.seconds}s supervisor cap nor the {args.watchdog}s frame "
                      f"watchdog ended the process within {args.seconds + grace}s. Treat this as a HANG "
                      f"whose own kill path failed, NOT a pass. Killed the whole process group: "
                      f"{killed} process(es) signalled, {alive} still alive afterwards "
                      f"({'clean' if alive == 0 else 'ORPHANS REMAIN — kill them by PID, never by name'}). "
                      f"Captured {out.count(chr(10))} output line(s) (stdout+stderr) before the kill. "
                      f"Log: {logpath}")
    secs = time.time() - t0
    out = (so or '') + (se or '')
    with open(logpath, 'w') as f:
        f.write(out)

    # gpuguard refusing preflight is a REFUSAL, not a failing gate: the game never launched.
    if not RE_BOOT_LOADED.search(out) and re.search(r'not launching|latch:.*(?:set|latched)', out):
        return refuse(f"the launcher refused before the port started — the game NEVER RAN, so nothing "
                      f"is proven. A gpuguard denial is a STOP, not an obstacle: diagnose statically "
                      f"and ask the user. Log: {logpath}\n" + out.strip()[-1200:])

    return analyse(out, rc, secs, 'boot', f'log: {logpath}')


def cmd_check_log(args) -> int:
    if not os.path.isfile(args.path):
        return refuse(f"{args.path} does not exist — NOTHING WAS ANALYSED.")
    out = open(args.path, errors='replace').read()
    # A captured log carries no exit code; the duration is the caller's claim about the run.
    return analyse(out, args.rc, args.seconds, 'check-log', f'analysed file: {args.path}')


# ------------------------------------------------------------------------------------------------
# --selftest — PROVE THE GATE FIRES. A gate nobody has seen fail is not a gate.
# The GOOD case is a real capture, quoted verbatim from a measured baseline run; each BAD case is that
# same text with one thing broken, so a failure is attributable to exactly that mutation.
# ------------------------------------------------------------------------------------------------
GOOD_LOG = """\
[gpuguard] INFO OK to run. Checked -- latch: clear; kernel log: 15 min scanned, 0 amdgpu trouble lines
[gpuguard] INFO launching (timeout 240s): ./scratch/bin/spiderman_port scratch/bin/spiderman/SLUS_008.75
[watchdog] armed: 30s frame-progress timeout (30s grace for the first frame)
[boot] loaded scratch/bin/spiderman/SLUS_008.75: entry 0x8008739C load 0x80010000 text 0xB6800 sp 0x801FFFF0
[plat-hle] 8 hardware-sync primitive(s) installed from GameConfig::hle
[rseam] default render leg = psx_render (reference). ... so PSXPORT_RENDER_PSX=0 renders scene '....' natively and then aborts at the next scene naming it. That abort is the correct result.
[rseam] render seam installed at 0x80061308 (the engine's submitFrame — the ONE game-side DrawOTag caller, RE-19/C029).
[cd] continuous-read pump installed on StGetNext (0x80086B10)
[cfg:warn] UNKNOWN knob PSXPORT_SPIDERMAN_DISC is set and matched nothing — it did NOTHING in this run
[render] render path = gte — geometry from the GUEST (its own GTE + ordering table)
[boot] Phase 0: dispatching guest main() 0x8002C354 on the recompiled substrate
[scene] FIRST scene identity: name='....' raw=00,00,00,00 code=0xFFFFFFD0 printable=0 (call #1, frame 2)
[rseam] submitFrame override REACHED — call #1 at frame 2, ra=80061218, leg=psx_render (reference), scene='....' code=0xFFFFFFD0
[rseam] submitFrame calls=512 frame=1214 leg=psx scene='....' code=0xFFFFFFD0 sceneChanges=1
[scene] CHANGED scene identity: name='titl' raw=74,69,74,6C code=0x0002 printable=1 (call #900, frame 2300)
[rseam] submitFrame calls=1024 frame=2400 leg=psx scene='titl' code=0x0002 sceneChanges=2
[scene] CHANGED scene identity: name='l1a1' raw=6C,31,61,31 code=0x0101 printable=1 (call #4891, frame 12525)
[rseam] submitFrame calls=5120 frame=12983 leg=psx scene='l1a1' code=0x0101 sceneChanges=9
[rseam] submitFrame calls=5632 frame=14007 leg=psx scene='l1a1' code=0x0101 sceneChanges=9

[watchdog] INTERRUPT (SIGINT/SIGTERM) — where it was stuck:
./scratch/bin/spiderman_port(_Z17gpu_pace_subframeP4Corei+0x33) [0x7f9683]
[gpuguard] INFO exit=130 after 240s (wall-clock timeout after 240s)
"""


def selftest() -> int:
    cases: list[tuple[str, str, int, int, float]] = []   # name, text, rc, expected_exit, secs

    cases.append(("GOOD baseline capture (must PASS)", GOOD_LOG, 130, 0, 240))

    # 1. the seam never fires — the exact regression the port's own install line warns about
    cases.append(("seam installed but NEVER FIRED",
                  GOOD_LOG.replace("[rseam] submitFrame override REACHED", "[rseam] (no override line)"),
                  130, 1, 240))
    # 2. the seam is not wired at all
    cases.append(("render seam NOT installed",
                  GOOD_LOG.replace("[rseam] render seam installed at", "[rseam] seam absent at"),
                  130, 1, 240))
    # 3. the boot executable never loaded
    cases.append(("boot exe never loaded",
                  GOOD_LOG.replace("[boot] loaded scratch", "[boot] xxxxxx scratch"), 130, 1, 240))
    # 4. counters frozen — boot stalls with the frame counter stuck
    frozen = GOOD_LOG
    for a, b in (("calls=1024 frame=2400", "calls=512 frame=1214"),
                 ("calls=5120 frame=12983", "calls=512 frame=1214"),
                 ("calls=5632 frame=14007", "calls=512 frame=1214")):
        frozen = frozen.replace(a, b)
    cases.append(("counters did not advance", frozen, 130, 1, 240))
    # 5. advanced, but far too slowly for the duration (the rate floor)
    cases.append(("advance far below the baseline rate",
                  GOOD_LOG.replace("calls=5632 frame=14007", "calls=1030 frame=2410")
                          .replace("calls=5120 frame=12983", "calls=1028 frame=2405")
                          .replace("(call #4891, frame 12525)", "(call #1027, frame 2404)"),
                  130, 1, 240))
    # 6. never left the boot-init scene '....'
    noscene = re.sub(r"\[scene\] CHANGED[^\n]*\n", "", GOOD_LOG)
    noscene = noscene.replace("sceneChanges=2", "sceneChanges=1").replace("sceneChanges=9", "sceneChanges=1")
    cases.append(("never reached a named scene", noscene, 130, 1, 240))
    # 7. the pc_render fail-fast fired (a real, expected-in-practice failure shape)
    cases.append(("the seam's unimplemented-scene abort fired",
                  GOOD_LOG + "[FATAL:error] \nunimplemented native rendering: no native producer "
                             "exists for this scene\n", 134, 1, 240))
    # 8. a recompiler miss
    cases.append(("recomp MISS", GOOD_LOG + "[dispatch] recomp-MISS at 0x800C6684\n", 134, 1, 240))
    # 9. the frame-progress watchdog fired — a real hang, not the ordinary cap
    cases.append(("frame watchdog STUCK",
                  GOOD_LOG + "\n[watchdog] STUCK: no frame presented within the timeout — backtrace:\n",
                  134, 1, 240))
    # 10. the guest loop omitted the unified render queue's per-game-frame fence
    cases.append(("captured render queue crossed frames until it overflowed",
                  GOOD_LOG + "[fps60:error] Fps60::rq_capture OVERFLOW: 65291 captured + 312 "
                             "this flush > FPS60_RQ_MAX 65536. Raise the cap; do not drop prims.\n",
                  134, 1, 240))
    # 11. an unexplained non-zero exit
    cases.append(("unexplained exit code", GOOD_LOG, 1, 1, 240))
    # 12. GPU device loss must be its OWN verdict (exit 3), not a mere FAIL
    cases.append(("GPU device loss => exit 3",
                  GOOD_LOG + "radv/amdgpu: The CS has been cancelled because the context is lost.\n",
                  130, 3, 240))
    # 13. no output at all must REFUSE (exit 2), never pass
    cases.append(("zero output => REFUSE", "", None, 2, 240))
    # 14. too few progress lines to speak about advance must REFUSE, not pass
    short = re.sub(r"\[rseam\] submitFrame calls=(?:1024|5120|5632)[^\n]*\n", "", GOOD_LOG)
    cases.append(("one progress line => REFUSE", short, 130, 2, 240))
    # 15. A SHORT log that ALSO carries a real abort must FAIL, not refuse. This case exists because the
    # first version of this gate got it wrong on a REAL log (scratch/re20/logs/pcleg_final.log): the
    # pc_render leg aborts at submitFrame call #2, i.e. long before a periodic progress line, and the
    # short-log refusal fired ahead of the pattern check and reported "nothing proven" over a FATAL it
    # had already read.
    cases.append(("short log WITH a real abort => FAIL, not REFUSE",
                  short + "[FATAL:error] \nunimplemented native rendering: no native producer exists "
                          "for this scene\n", 134, 1, 240))

    print("=" * 96)
    print("SELFTEST — every case below is judged by the SAME analyse() the real gate uses.")
    print("Expected exits: 0 = pass, 1 = fail, 2 = refuse (nothing proven), 3 = GPU device loss.")
    print("=" * 96)
    bad = 0
    for i, (name, text, rc, want, secs) in enumerate(cases, 1):
        print(f"\n----- case {i}/{len(cases)}: {name}  (expect exit {want}) -----")
        got = analyse(text, rc, secs, f'self{i}')
        verdict = 'OK' if got == want else 'SELFTEST FAILURE'
        if got != want:
            bad = 1
        print(f"----- case {i}: exit {got}, expected {want} -> {verdict}")

    # LAST CASE: the missing-binary refusal, exercised through the REAL launcher path (cmd_boot) rather
    # than through analyse(), so the refusal that guards every real invocation is itself covered.
    print(f"\n----- case {len(cases) + 1}: missing binary via the real launcher (expect exit 2) -----")
    global BIN
    saved, BIN = BIN, os.path.join(REPO, 'scratch', 'bin', 'does_not_exist_selftest')
    got = cmd_boot(argparse.Namespace(seconds=5, grace=4, watchdog=5, debug='', no_gpuguard=True))
    BIN = saved
    print(f"----- case {len(cases) + 1}: exit {got}, expected 2 -> "
          f"{'OK' if got == 2 else 'SELFTEST FAILURE'}")
    if got != 2:
        bad = 1

    # HANG-PATH CASE, through the REAL launcher. Both bugs found in the 2026-08-12 audit lived in this
    # branch and NOTHING covered it: (a) it saved only stdout, and the port writes 100% of its output to
    # stderr, so the refusal pointed at an EMPTY log; (b) it killed only the direct child, orphaning a
    # GPU-holding process. The stand-in prints to stderr like the port does and spawns a grandchild like
    # gpuguard does, so the case fails if either bug returns.
    print(f"\n----- case {len(cases) + 2}: a HANG via the real launcher (expect exit 2, a NON-EMPTY log, "
          f"and NO surviving process) -----")
    stand_in = os.path.join(REPO, 'scratch', 'bin', 'gate_selftest_hang.sh')
    beat = os.path.join(REPO, 'scratch', 'bin', 'gate_selftest_heartbeat')
    pidf = os.path.join(REPO, 'scratch', 'bin', 'gate_selftest_orphan.pid')
    os.makedirs(os.path.dirname(stand_in), exist_ok=True)
    for f_ in (beat, pidf):
        if os.path.exists(f_):
            os.unlink(f_)
    # HOW THE SURVIVOR IS DETECTED, and why not by argv. Two earlier versions of this check could not
    # fire: scanning `ps` for the script's NAME missed the grandchild (whose argv is 'sleep 600'), and
    # putting a marker in `sh -c 'sleep 600 # MARK'` failed because dash EXEC-OPTIMISES that into bare
    # 'sleep 600', erasing the marker. Both duly printed "surviving=0" against the pre-fix launcher,
    # which MEASURABLY leaks 2 orphans per run. So the grandchild instead publishes its own PID and
    # TOUCHES A HEARTBEAT FILE in a loop: an advancing mtime after the gate returned is proof something
    # is still running, and the PID file makes the cleanup a kill BY PID rather than by name.
    with open(stand_in, 'w') as f:
        f.write('#!/bin/sh\n'
                'echo "SELFTEST-STAND-IN: this line goes to STDERR, as every port log line does" >&2\n'
                f"sh -c 'echo $$ > {pidf}; while : ; do : > {beat}; sleep 0.2; done' &\n"
                'wait\n')      # `wait`, not another `sleep`: one child only, so any leak has a known PID
    os.chmod(stand_in, 0o755)
    saved, BIN = BIN, stand_in
    os.makedirs(LOGDIR, exist_ok=True)
    LOGS_BEFORE = sorted(os.listdir(LOGDIR))
    got = cmd_boot(argparse.Namespace(seconds=3, grace=4, watchdog=5, debug='', no_gpuguard=True))
    BIN = saved
    newlogs = [x for x in sorted(os.listdir(LOGDIR)) if x not in LOGS_BEFORE] if os.path.isdir(LOGDIR) else []
    logtext = open(os.path.join(LOGDIR, newlogs[-1])).read() if newlogs else ''
    spawned = os.path.exists(beat)      # the DENOMINATOR: did the grandchild ever run at all?
    m0 = os.stat(beat).st_mtime_ns if spawned else 0
    time.sleep(1.5)
    m1 = os.stat(beat).st_mtime_ns if os.path.exists(beat) else m0
    alive = spawned and m1 != m0
    ok_exit = got == 2
    ok_log = 'SELFTEST-STAND-IN' in logtext
    ok_dead = spawned and not alive
    print(f"      exit={got} (want 2) · log captured {logtext.count(chr(10))} line(s), stderr marker "
          f"present={ok_log} · grandchild spawned={spawned}, heartbeat still advancing after the "
          f"gate returned={alive} (want False)")
    if not spawned:
        print("      SELFTEST FAILURE: the grandchild never started, so the orphan half of this case "
              "proved NOTHING — it did not observe a clean kill, it observed nothing.")
    if not ok_log:
        print("      SELFTEST FAILURE: the hang refusal saved a log WITHOUT the process's stderr — a "
              "refusal pointing at an empty file is a diagnostic that can print nothing.")
    if alive:
        print("      SELFTEST FAILURE: a process SURVIVED the gate's hang path — the gate orphaned it "
              "instead of killing its process group. An orphan holding the GPU is what the next run "
              "then contends with.")
    if os.path.exists(pidf):            # clean up BY PID, never by binary name
        try:
            os.kill(int(open(pidf).read().strip()), signal.SIGKILL)
        except (ProcessLookupError, ValueError, PermissionError):
            pass
    print(f"----- case {len(cases) + 2}: "
          f"{'OK' if (ok_exit and ok_log and ok_dead) else 'SELFTEST FAILURE'}")
    if not (ok_exit and ok_log and ok_dead):
        bad = 1
    for f_ in (stand_in, beat, pidf):
        if os.path.exists(f_):
            os.unlink(f_)

    # The denominator is COUNTED, not asserted in prose: a hand-written tally drifts the moment a case
    # is added, and a wrong denominator is exactly the lie this whole file is written against.
    n = len(cases) + 2
    n_pass = sum(1 for _n, _t, _r, w, _s in cases if w == 0)
    n_fail = sum(1 for _n, _t, _r, w, _s in cases if w == 1)
    # +2: the two REFUSALS exercised through the real launcher (missing binary, and a hang)
    n_refuse = sum(1 for _n, _t, _r, w, _s in cases if w == 2) + 2
    n_dev = sum(1 for _n, _t, _r, w, _s in cases if w == 3)
    print("\n" + "=" * 96)
    if bad:
        print(f"SELFTEST FAILED over {n} case(s): the gate does not judge as documented. Do not trust "
              f"any verdict it produced.")
        return 1
    print(f"SELFTEST PASSED over {n} case(s): {n_pass} known-good capture(s) passed, and "
          f"{n - n_pass} broken variants were caught — {n_fail} FAIL, {n_refuse} REFUSE, "
          f"{n_dev} device-loss STOP — each keyed on exactly one mutated line. "
          f"THE GATE HAS BEEN SEEN TO FAIL.")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--selftest', action='store_true',
                    help='prove the gate fires: judge a known-good capture plus one mutation per '
                         'assertion, and a real missing-binary refusal')
    sub = ap.add_subparsers(dest='cmd')
    b = sub.add_parser('boot', help='capped headless launch + assertions (THE gate)')
    b.add_argument('--seconds', type=int, default=120,
                   help='wall-clock cap for the run (default 120). The floors scale with it; the port '
                        'prints a progress line every 512 submitFrame calls, so a very short run '
                        'REFUSES for want of two lines rather than passing on one.')
    b.add_argument('--watchdog', type=int, default=30,
                   help="PSXPORT_WATCHDOG: the port's own frame-progress timeout in seconds "
                        "(default 30). A stall aborts in-band with a guest backtrace.")
    b.add_argument('--grace', type=int, default=120,
                   help='seconds to wait for the run to end BEYOND --seconds before declaring a hang '
                        'and group-killing it (default 120: gpuguard\'s own cap plus the port\'s '
                        'shutdown). A hang is a REFUSAL, never a pass.')
    b.add_argument('--debug', default='', help='PSXPORT_DEBUG channels, e.g. rseam,envcheck')
    b.add_argument('--no-gpuguard', action='store_true',
                   help='launch without the machine-wide GPU interlock. Only for a machine that has '
                        'no gpuguard — never to route around a denial.')
    b.set_defaults(fn=cmd_boot)
    c = sub.add_parser('check-log', help='run the SAME analyser over a captured log')
    c.add_argument('path')
    c.add_argument('--seconds', type=float, default=240, help="the captured run's duration")
    c.add_argument('--rc', type=int, default=None, help="the captured run's exit code, if known")
    c.set_defaults(fn=cmd_check_log)
    args = ap.parse_args()
    if args.selftest:
        return selftest()
    if not args.cmd:
        ap.print_help()
        return 2
    return args.fn(args)


if __name__ == '__main__':
    sys.exit(main())
