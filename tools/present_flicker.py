#!/usr/bin/env python3
"""present_flicker.py — is a run's PRESENTED PICTURE oscillating? Per-present colour count + a
per-pixel diff of every CONSECUTIVE pair, over a capture window that the run's own log defines.

WHY THIS TOOL EXISTS, and what it refuses to do
-----------------------------------------------
Two instrument failures in this repo motivated every design choice here, so they are stated rather
than left to be rediscovered:

  * "NON-BLACK %" CANNOT SEE FLICKER. It read 93.3% on BOTH the good and the flat frames of spyro's
    alternating composite — the two frames it was supposed to tell apart. Coverage is a measure of
    how much of the screen is lit, not of whether the content changed. This tool reports DISTINCT
    COLOUR COUNT per present and a PER-PIXEL DIFF between consecutive presents instead, and it prints
    non-black only as a cross-check against the capture's own log line.

  * A GLOB OVER scratch/screenshots/ IS NOT A RESULT SET. That directory is a shared accumulator
    every run in this repo writes into. A STALE file swept in by a wildcard was mistaken for "the
    correct frame" and produced the (since refuted) widescreen explanation of Spider-Man's flicker.
    So this tool takes a MANIFEST — the run's own `[present_shot] wrote <path> ... non-black a/b` log
    lines — and reads exactly those files, in that order. It NEVER lists a directory. A file present
    on disk but absent from the log is not analysed; a file named in the log but missing on disk is a
    hard error, not a skipped row.

WHAT A NEGATIVE PRINTS, decided before the checks were written
--------------------------------------------------------------
The failure mode this project keeps hitting is a diagnostic whose "nothing found" is indistinguishable
from "I never looked". So a clean run here prints its DENOMINATORS — presents compared, pixels per
present, total pixel comparisons — on the same line as the zero, and the summary always names what
the method cannot see. `--selftest` feeds it a synthetic pair that MUST come back as oscillating, so
"0 oscillating pixels" is only ever printed by a checker just proven able to print a non-zero.

  python3 tools/present_flicker.py --log scratch/logs/g6/shotB.log --dir scratch/shots-g6/B_rest
  python3 tools/present_flicker.py --selftest

BLIND SPOTS, stated because a reader cannot infer them from the output:
  * It sees only what `gpu_vk_present_shot` captured — the composited present image. Anything wrong
    BEFORE compositing (a guest VRAM defect that composites away) is invisible here.
  * It compares presents that the PORT produced. It cannot see which of them the COMPOSITOR actually
    scanned out: under a MAILBOX swapchain a present submitted a millisecond after its predecessor
    replaces it unseen. Pictorial identity of consecutive presents therefore does NOT imply the
    screen was stable — pair that conclusion with the present-cadence measurement
    (PSXPORT_DEBUG=presentclock), never with this tool alone.
  * A window of N presents is N/60 s of one scene. It says nothing about any other scene.
"""
import argparse
import os
import re
import sys

LOGLINE = re.compile(r'\[present_shot\] wrote (\S+) \((\d+)x(\d+) [^)]*\) non-black (\d+)/(\d+)')


def read_ppm(path):
    """Minimal binary-P6 reader. Refuses anything it does not fully understand — a silently
    mis-parsed image would produce confident wrong pixel counts."""
    with open(path, 'rb') as f:
        data = f.read()
    if not data.startswith(b'P6'):
        raise ValueError('%s is not a binary P6 PPM' % path)
    fields, pos = [], 2
    while len(fields) < 3:
        while pos < len(data) and data[pos:pos + 1].isspace():
            pos += 1
        if data[pos:pos + 1] == b'#':
            while pos < len(data) and data[pos:pos + 1] != b'\n':
                pos += 1
            continue
        start = pos
        while pos < len(data) and not data[pos:pos + 1].isspace():
            pos += 1
        fields.append(int(data[start:pos]))
    pos += 1
    w, h, maxv = fields
    if maxv != 255:
        raise ValueError('%s has maxval %d; only 8-bit is supported' % (path, maxv))
    px = data[pos:pos + w * h * 3]
    if len(px) != w * h * 3:
        raise ValueError('%s is truncated: %d of %d payload bytes' % (path, len(px), w * h * 3))
    return w, h, px


def manifest(log_path, want_dir):
    """The result set is whatever the RUN said it wrote, in order — never a directory listing."""
    rows = []
    with open(log_path, encoding='utf-8', errors='replace') as f:
        for line in f:
            m = LOGLINE.search(line)
            if not m:
                continue
            path, w, h, nb, tot = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5))
            if want_dir and os.path.normpath(os.path.dirname(path)) != os.path.normpath(want_dir):
                continue
            rows.append((path, w, h, nb, tot))
    return rows


def identity_sequence(stats):
    """THE PRIMARY FLICKER MEASURE: which DISTINCT PICTURE each present carried, in order.

    This exists because the per-pixel oscillation count below turned out to be untrustworthy on real
    content and nearly published a wrong answer. MEASURED on a 120-present Spider-Man gameplay
    capture: per-pixel oscillation said 48.1% of the frame was "oscillating", while the frame
    identities were 39 strictly-new pictures with exactly 2 revisits and NO alternation. The cause is
    that these frames hold only ~1500 distinct colours over 691200 pixels, so a pixel returning to a
    colour it held four frames ago is overwhelmingly coincidence, not flicker. A metric whose false
    positives look like the defect is worse than no metric.

    Frame identity has no such failure mode: pictures are compared whole, so "picture 12 came back
    after pictures 13-16" is a statement about the composite and nothing else. Flicker is
    ALTERNATION (A B A B); ordinary animation is strictly-new pictures; a stalled scene is one
    picture repeated. The three are distinguishable here and are not distinguishable by pixel counts.
    """
    ids, seq = {}, []
    for s in stats:
        seq.append(ids.setdefault(bytes(s['px']), len(ids)))
    runs = []
    for v in seq:
        if runs and runs[-1][0] == v:
            runs[-1][1] += 1
        else:
            runs.append([v, 1])
    seen, revisits = set(), []
    for i, v in enumerate(seq):
        if v in seen and i and seq[i - 1] != v:
            revisits.append(i)
        seen.add(v)
    return seq, runs, revisits


def analyse(rows, oscillate_window):
    stats = []
    for path, w, h, nb, tot in rows:
        if not os.path.exists(path):
            sys.exit('MISSING: %s is named in the log but not on disk — the capture set is '
                     'incomplete, so no result is reported.' % path)
        iw, ih, px = read_ppm(path)
        if (iw, ih) != (w, h):
            sys.exit('SIZE MISMATCH: %s is %dx%d on disk but the log recorded %dx%d.'
                     % (path, iw, ih, w, h))
        colours = len({px[i:i + 3] for i in range(0, len(px), 3)})
        nonblack = sum(1 for i in range(0, len(px), 3) if px[i] or px[i + 1] or px[i + 2])
        if nonblack != nb:
            sys.exit('LOG DISAGREES WITH FILE: %s non-black is %d here, %d in the log. The file on '
                     'disk is not the one that run wrote.' % (path, nonblack, nb))
        stats.append({'path': path, 'w': iw, 'h': ih, 'colours': colours, 'nonblack': nb,
                      'total': iw * ih, 'px': px})

    diffs = []
    for a, b in zip(stats, stats[1:]):
        pa, pb = a['px'], b['px']
        n = sum(1 for i in range(0, len(pa), 3) if pa[i:i + 3] != pb[i:i + 3])
        diffs.append(n)

    # An OSCILLATING pixel is one that changes and then changes BACK to a value it already held
    # inside the window — the signature of flicker, as distinct from ordinary animation, where a
    # pixel changes and stays changed. Bounded to `oscillate_window` presents so the memory cost is
    # bounded on a long capture.
    osc = 0
    if len(stats) >= 3:
        k = min(oscillate_window, len(stats))
        npx = stats[0]['total']
        for i in range(npx):
            seen, last, flips = set(), None, 0
            for s in stats[:k]:
                v = s['px'][i * 3:i * 3 + 3]
                if v != last:
                    if v in seen:
                        flips += 1
                    seen.add(v)
                    last = v
            if flips:
                osc += 1
    return stats, diffs, osc


def report(stats, diffs, osc, oscillate_window):
    npx = stats[0]['total']
    cols = [s['colours'] for s in stats]
    print('CAPTURE SET   %d presents, %dx%d = %d px each, from the run log (not a glob)'
          % (len(stats), stats[0]['w'], stats[0]['h'], npx))
    print('              every file re-verified against its own [present_shot] non-black count')
    print()

    seq, runs, revisits = identity_sequence(stats)
    ndist = len(set(seq))
    print('FRAME IDENTITY (the primary flicker measure)')
    print('              %d distinct pictures across %d presents -> %.1f new pictures/s at 59.94 presents/s'
          % (ndist, len(seq), 59.94 * len(runs) / len(seq)))
    print('              hold lengths (presents each picture stayed on): %s'
          % ', '.join(str(r[1]) for r in runs[:30]) + (' ...' if len(runs) > 30 else ''))
    print('              REVISITS (a picture returning after a different one intervened): %d of %d presents'
          % (len(revisits), len(seq)))
    if not revisits:
        print('              => NO alternation. Every picture change is monotonic (animation), not flicker.')
    else:
        print('              => at presents %s — inspect these; alternation is A B A B, a returning'
              % (', '.join(str(i) for i in revisits[:12]) + (' ...' if len(revisits) > 12 else '')))
        print('                 animation loop is not.')
    print()
    print('DISTINCT COLOURS PER PRESENT   min=%d max=%d mean=%.1f  distinct values across window=%d'
          % (min(cols), max(cols), sum(cols) / len(cols), len(set(cols))))
    if len(set(cols)) == 1:
        print('              every present has the SAME colour count (%d)' % cols[0])
    else:
        print('              per-present counts: %s' % (', '.join(str(c) for c in cols[:24])
                                                        + (' ...' if len(cols) > 24 else '')))
    print()
    changed = sum(1 for d in diffs if d)
    print('CONSECUTIVE PER-PIXEL DIFF     %d pairs compared, %d px per comparison, %d total px comparisons'
          % (len(diffs), npx, len(diffs) * npx))
    print('              pairs that differ at all: %d of %d (%.1f%%)'
          % (changed, len(diffs), 100.0 * changed / len(diffs) if diffs else 0.0))
    if diffs:
        print('              differing px per pair: min=%d max=%d (%.3f%% of frame) mean=%.1f'
              % (min(diffs), max(diffs), 100.0 * max(diffs) / npx, sum(diffs) / len(diffs)))
    print()
    k = min(oscillate_window, len(stats))
    print('OSCILLATING PIXELS (WEAK — read FRAME IDENTITY above instead)')
    print('              %d of %d (%.4f%%) over the first %d presents' % (osc, npx, 100.0 * osc / npx, k))
    print('              DO NOT read this as a flicker rate. These frames carry only ~%d distinct'
          % (sum(cols) / len(cols)))
    print('              colours over %d pixels, so a pixel revisiting an earlier colour is mostly' % npx)
    print('              coincidence: this metric reported 48.1%% on a capture whose frame identities')
    print('              showed 39 strictly-new pictures and zero alternation. Kept only as a')
    print('              locator for WHERE to look once FRAME IDENTITY says something is alternating.')
    print()
    print('WHAT THIS RUN DOES NOT COVER: one scene, %.2f s of it; the composited present only; and '
          'which presents the COMPOSITOR scanned out is not observable here — read it with the '
          'presentclock cadence measurement, not alone.' % (len(stats) / 59.94))


def selftest():
    """Feed the checker a case that MUST come back positive, and one that MUST come back negative.
    A discriminator is only trustworthy once it has been run against BOTH classes."""
    import tempfile
    ok = True
    with tempfile.TemporaryDirectory() as td:
        def write(name, px, w=4, h=2):
            p = os.path.join(td, name)
            with open(p, 'wb') as f:
                f.write(b'P6\n%d %d\n255\n' % (w, h))
                f.write(bytes(px))
            return p, w, h, sum(1 for i in range(0, len(px), 3) if px[i] or px[i + 1] or px[i + 2]), w * h

        # POSITIVE: pixel 0 alternates red/black across 4 presents; everything else is constant.
        red = [255, 0, 0] + [10, 10, 10] * 7
        blk = [0, 0, 0] + [10, 10, 10] * 7
        rows = [write('a0.ppm', red), write('a1.ppm', blk), write('a2.ppm', red), write('a3.ppm', blk)]
        _, d, osc = analyse(rows, 8)
        print('selftest POSITIVE (1 px alternating): oscillating=%d  diffs=%s' % (osc, d))
        if osc != 1 or d != [1, 1, 1]:
            print('  FAIL — the checker cannot see a flicker it was handed'); ok = False

        # NEGATIVE: a pixel that changes ONCE and stays changed is animation, not flicker.
        rows = [write('b0.ppm', red), write('b1.ppm', red), write('b2.ppm', blk), write('b3.ppm', blk)]
        _, d, osc = analyse(rows, 8)
        print('selftest NEGATIVE (1 px monotonic):  oscillating=%d  diffs=%s' % (osc, d))
        if osc != 0 or d != [0, 1, 0]:
            print('  FAIL — the checker calls ordinary animation flicker'); ok = False
    print('selftest %s' % ('PASSED — it can print both answers' if ok else 'FAILED'))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--log', help='run log containing the [present_shot] wrote ... lines')
    ap.add_argument('--dir', help='restrict to captures written into this directory')
    ap.add_argument('--oscillate-window', type=int, default=40)
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()
    if a.selftest:
        return selftest()
    if not a.log:
        ap.error('--log is required (the manifest); this tool never lists a directory')
    rows = manifest(a.log, a.dir)
    if not rows:
        sys.exit('REFUSING TO REPORT: %s contains no "[present_shot] wrote" lines%s. Nothing was '
                 'analysed — this is NOT a clean result.'
                 % (a.log, (' for directory %s' % a.dir) if a.dir else ''))
    stats, diffs, osc = analyse(rows, a.oscillate_window)
    report(stats, diffs, osc, a.oscillate_window)
    return 0


if __name__ == '__main__':
    sys.exit(main())
