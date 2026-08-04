#!/usr/bin/env bash
# Fully automated build-and-run for the Spider-Man native PC port (Linux + macOS).
#
#   ./run.sh [/path/to/Spider-Man.chd]
#
# End to end: builds the CHD tooling (libchdr + discdump), extracts the boot executable from your
# disc, statically recompiles it, builds the native port, and launches it. The disc image is yours
# to provide and is never shipped — pass it as an argument, set PSXPORT_SPIDERMAN_DISC, put it in a
# .env file, or drop a *.chd next to this script.
#
# Requirements (install once):
#   Linux:  cmake pkg-config SDL3-devel libzstd-devel zlib-devel python3 gcc-c++
#   macOS:  brew install cmake pkg-config sdl3 zstd zlib python3
#
# Env knobs: PSXPORT_NOAUDIO=1 (mute), PSXPORT_NOWINDOW=1 (headless), CC=clang/gcc,
#            PSXPORT_DEBUG=chan,chan (channel-gated diagnostics — `vsync` is a useful one),
#            PSXPORT_WATCHDOG=<sec> (abort + backtrace if no frame is presented in time).
#
# no pipefail: several steps use `cmd | head -1`, where head closing early would SIGPIPE the
# producer and (under pipefail) abort the script; results are validated explicitly instead.
set -eu
cd "$(dirname "$0")"

say() { printf '\033[1;36m[run]\033[0m %s\n' "$*"; }
die() { printf '\033[1;31m[run] error:\033[0m %s\n' "$*" >&2; exit 1; }

# ---- 0. toolchain -------------------------------------------------------------------
command -v cmake      >/dev/null || die "cmake not found"
command -v python3    >/dev/null || die "python3 not found"
command -v pkg-config >/dev/null || die "pkg-config not found"
pkg-config --exists sdl3 || die "SDL3 not found (Linux: SDL3-devel / libsdl3-dev; macOS: brew install sdl3)"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# ---- 0b. sync git submodules --------------------------------------------------------
# A plain `git pull` does NOT update submodules, so after a pull the framework (and the beetle
# backend nested under it) can be stale and the link fails with undefined GTE/MDEC/SPU symbols.
# Sync here so `git pull && ./run.sh` is self-sufficient.
#
# ONE implementation, shared by all three ports: external/psxport/scripts/sync-submodules.sh. The
# copy that used to live here guarded only `external/psxport` for uncommitted work and then updated
# EVERY submodule, and it never said which shas it moved — so a sync that reverted a deliberately
# checked-out commit was indistinguishable from a no-op. See that script's header for what that cost.
#
# Bootstrap: the script lives INSIDE the submodule, so on a fresh clone (or against a gitlink older
# than the script itself) it does not exist yet — init first, then call it.
if command -v git >/dev/null && [ -f .gitmodules ]; then
  if [ ! -f external/psxport/scripts/sync-submodules.sh ]; then
    say "initializing git submodules…"
    git submodule update --init --recursive || die "git submodule update failed"
  fi
  if [ -f external/psxport/scripts/sync-submodules.sh ]; then
    bash external/psxport/scripts/sync-submodules.sh || die "submodule sync failed"
  else
    say "WARNING: external/psxport/scripts/sync-submodules.sh is absent even after init —"
    say "         submodules were NOT synced and may not match this repo's recorded gitlinks."
  fi
fi

# ---- 1. resolve the disc ------------------------------------------------------------
DISC="${1:-${PSXPORT_SPIDERMAN_DISC:-}}"
if [ -z "$DISC" ] && [ -f .env ]; then
  DISC="$(sed -n 's/^[[:space:]]*PSXPORT_SPIDERMAN_DISC[[:space:]]*=[[:space:]]*//p' .env | head -1)"
  [ -z "$DISC" ] && DISC="$(sed -n 's/^[[:space:]]*PSXPORT_DISC[[:space:]]*=[[:space:]]*//p' .env | head -1)"
fi
if [ -z "$DISC" ]; then
  DISC="$(ls ./*.chd 2>/dev/null | head -1 || true)"
fi
[ -n "$DISC" ] && [ -f "$DISC" ] || die "no disc image — pass it as ./run.sh <disc.chd>, set PSXPORT_SPIDERMAN_DISC, or drop a *.chd here"
say "disc: $DISC"

# ---- 2. build the CHD tooling (libchdr + discdump) ----------------------------------
# ALWAYS (re)build discdump: CMake is incremental (fast when up to date), and a STALE binary is a
# silent trap — an old discdump that cannot walk this disc's layout extracts nothing and the recomp
# is built from a different input set than everyone else's. discdump is a FRAMEWORK tool, so it
# builds from the submodule, not this repo.
say "building libchdr + discdump…"
cmake -S external/psxport -B external/psxport/build -DCMAKE_BUILD_TYPE=Release >/dev/null \
  || die "psxport cmake configure failed"
cmake --build external/psxport/build -j "$JOBS" --target discdump >/dev/null \
  || die "discdump build failed"
DISCDUMP=external/psxport/build/tools/discdump
[ -x "$DISCDUMP" ] || DISCDUMP=external/psxport/build/tools/discdump.exe
[ -x "$DISCDUMP" ] || die "discdump build failed"

# ---- 3. ensure the recompiled substrate is present AND matches its input hash --------
# ONE step does all recomp provisioning: extract SLUS_008.75, run emit.py, and verify the generated
# set matches a deterministic hash of the inputs, so every machine builds byte-identical recomp.
# See tools/ensure_recomp.py.
EXE=scratch/bin/spiderman/SLUS_008.75
mkdir -p generated scratch/bin
PSXPORT_DISCDUMP="$DISCDUMP" python3 tools/ensure_recomp.py "$DISC" || die "recomp provisioning failed"
[ -f "$EXE" ] || die "ensure_recomp.py did not produce $EXE"

# ---- 4. build the native port -------------------------------------------------------
# CMake owns the whole port build (cmake/spiderman_port.cmake): the game seam, the substrate shards,
# the SDL_GPU shader generation, and the SDL3 link. Configure is idempotent; the build is incremental.
say "building the native port (CMake -j$JOBS)…"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null || die "cmake configure failed"
cmake --build build -j "$JOBS" --target spiderman_port || die "port build failed"

# ---- 5. run -------------------------------------------------------------------------
say "launching Spider-Man (native PC port)…"
# run.sh is the user's WINDOWED entry point, so it explicitly opts into a window. The binary itself
# is HEADLESS by default so agent/CI runs that forget the flag fail safe (no intrusive window)
# rather than popping one. PSXPORT_NOWINDOW keeps run.sh headless.
if [ -n "${PSXPORT_NOWINDOW:-}" ]; then export PSXPORT_VK_HEADLESS=1; else export PSXPORT_VK_WINDOW=1; fi
# (The boot-FMV workaround that used to live here is gone. The framework no longer hardcodes the
# reference consumer's movie path — it reads GameConfig::bootFmv, which this port leaves empty
# because its boot runs on the substrate and the guest plays its own movies. Nothing to suppress.)
# RmlUi debug/mod overlay assets (fonts + menu.rml) ship with the FRAMEWORK, and the overlay loads
# them relative to PSXPORT_ASSET_DIR (the dir CONTAINING assets/). We run from the repo root, so
# point it at the submodule.
export PSXPORT_ASSET_DIR="${PSXPORT_ASSET_DIR:-external/psxport}"

PSXPORT_SPIDERMAN_DISC="$DISC" exec ./scratch/bin/spiderman_port "$EXE"
