#!/usr/bin/env python3
"""wide_pair.py — capture a MATCHED 4:3 / 16:9 pair from the same game state, then rule on it.

THE PROBLEM THIS EXISTS TO AVOID. `widescreen_pair.py` refuses a pair whose two images have the same
width with "the wide one must be wider. NOTHING WAS COMPARED", and that is the correct refusal: a 4:3
run and a wide run that both land on the same sink size produce two identical pictures, and a verdict
computed from them is not a measurement. The sink is therefore SIZED PER ASPECT here, from the
`present_plan.h` rule (`aspect = (4/3) * (disp_w / native_w)`), so the wide leg is genuinely wider
before the comparison is attempted at all.

IT NEVER RUNS A PRODUCT. It shells out to nothing and starts nothing; the caller runs the product,
because a machine-wide single-instance rule belongs to whoever owns the machine. It emits the two
command lines, waits for the files, and then runs the framework's own discriminator — so the verdict is
the framework's, quoted, and this file only guarantees the pair is comparable.

    uv run --frozen python tools/wide_pair.py --plan
    uv run --frozen python tools/wide_pair.py --verify scratch/wide
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# The title's own 4:3 framebuffer width, from the GP1 display environment 0x80061140 published at boot
# (`addiu $a3,$zero,512` at 0x8006115C/0x80061188/0x800611A0/0x800611B8). Not a framework default: a
# 512-wide game is exactly the case the framework's own `present_plan.h` comment names when it
# explains why 320 is not a safe assumption.
NATIVE_WIDTH = 512
NATIVE_HEIGHT = 240

# The sink height both legs share. Only the WIDTH differs, and it differs by the canvas ratio, which
# is what makes the two captures comparable rather than merely two files.
SINK_HEIGHT = 720


def sink_width(aspect: str, native_width: int) -> int:
    ratio = {"4x3": 4 / 3, "16x9": 16 / 9}[aspect]
    # pane_letterbox(4*disp_w, 3*native_w) is the framework's own aspect expression, inverted: the
    # sink the wide leg needs is the ratio the widened frame asks for.
    return int(round(native_width * ratio))


def env_for(aspect: str, frames: int, shots: str, settings: Path) -> dict[str, str]:
    env = dict(os.environ)
    env.update(
        PSXPORT_VK_HEADLESS="1",
        PSXPORT_NOAUDIO="1",
        PSXPORT_NOPACE="1",
        PSXPORT_NATIVE_FRAMES=str(frames),
        PSXPORT_PRESENT_SHOT_AT=shots,
        PSXPORT_PRESENT_SINK=f"{sink_width(aspect, NATIVE_WIDTH)}x{SINK_HEIGHT}",
        PSXPORT_SETTINGS=str(settings),
        SDL_VIDEODRIVER="offscreen",
        SDL_AUDIODRIVER="dummy",
    )
    return env


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", action="store_true", help="print the two run command lines and exit")
    parser.add_argument("--verify", type=Path, help="two capture files, and run the framework's verdict")
    parser.add_argument("--frames", type=int, default=6000)
    parser.add_argument("--shots", default="5200,5400,5600,5800")
    parser.add_argument("--narrow", type=Path)
    parser.add_argument("--wide", type=Path)
    args = parser.parse_args()

    if args.plan:
        for aspect, settings in (
            ("4x3", ROOT / "scratch/wide/settings_4x3.ini"),
            ("16x9", ROOT / "scratch/wide/settings_16x9.ini"),
        ):
            width = sink_width(aspect, NATIVE_WIDTH)
            print(f"# {aspect}: sink {width}x{SINK_HEIGHT} from native {NATIVE_WIDTH}x{NATIVE_HEIGHT}")
            print(f"PSXPORT_PRESENT_SINK={width}x{SINK_HEIGHT} \\")
            print(f"  {ROOT / 'build/consumer-verify/bin/spiderman_port'}")
        return 0

    narrow = args.narrow
    wide = args.wide
    if args.verify:
        legs = {
            "4x3": sorted(args.verify.glob("present_*.png")),
            "16x9": sorted(args.verify.glob("wide_present_*.png")),
        }
        for aspect, found in legs.items():
            if not found:
                print(f"wide_pair: NO CAPTURE for the {aspect} leg under {args.verify} "
                      f"(looked for {list(legs)})", file=sys.stderr)
                return 2
            print(f"wide_pair: {aspect} leg has {len(found)} capture(s): "
                  f"{', '.join(path.name for path in found)}")
        narrow = legs["4x3"][-1]
        wide = legs["16x9"][-1]

    assert narrow is not None and wide is not None
    print(f"wide_pair: comparing {narrow.name} (4:3) with {wide.name} (16:9)")
    result = subprocess.run(
        [sys.executable, str(ROOT / "external/psxport/tools/port/widescreen_pair.py"),
         "--narrow", str(narrow), "--wide", str(wide)],
        check=False,
    )
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
