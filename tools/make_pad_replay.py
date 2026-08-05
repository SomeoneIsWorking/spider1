#!/usr/bin/env python3
"""make_pad_replay.py — build a DETERMINISTIC pad replay, so a present index means the same thing twice.

WHY (issue 0009). Driving the front-end with PSXPORT_FORCE_BUTTONS pulses CROSS on a WALL-CLOCK-ish
cadence, so where the pulse train lands depends on run timing: present 4500 was a corrupted city
skyline in one run and a pause menu in another. Every "capture frame N before, capture frame N after,
compare" check is invalid under that — you get two real frames and a real difference, and the
difference is the SCENE, not the change. A present index is a clock, not a bookmark.

PSXPORT_PAD_REPLAY fixes it: the file is one uint16 LE pad mask per pad frame, forced back verbatim,
so the same file gives the same inputs at the same frames on every run. This writes such a file with
the same pulse shape FORCE_BUTTONS uses (8 frames pressed, 24 released), which is what walks this
game's menus.

    python3 tools/make_pad_replay.py scratch/bin/drive.pad --frames 20000 --button 4000
    PSXPORT_PAD_REPLAY=scratch/bin/drive.pad ... ./run.sh

MASKS ARE ACTIVE-LOW: a bit that is 0 means PRESSED. Idle is 0xFFFF. --button takes the same hex a
human writes for PSXPORT_FORCE_BUTTONS (4000 = CROSS, 0040 = DOWN, 0008 = START) and this inverts it,
so the two knobs cannot disagree about polarity — getting that backwards yields a replay that holds
every button except the one you wanted, which looks like "the game ignored my input".

DETERMINISM IS A PROPERTY OF THE FILE, NOT A PROMISE: two runs with the same replay still share a
host, and anything the port derives from wall-clock (frame pacing, a timeout kill) can still differ.
This removes the INPUT as a source of divergence, which is the one that was actually biting. Verify
rather than assume — capture the same index twice and compare the bytes.
"""
import struct
import sys


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    if not args:
        print(__doc__)
        return 2
    out = args[0]
    frames, on, off, btn = 20000, 8, 24, 0x4000
    argv = sys.argv
    if '--frames' in argv:
        frames = int(argv[argv.index('--frames') + 1])
    if '--on' in argv:
        on = int(argv[argv.index('--on') + 1])
    if '--off' in argv:
        off = int(argv[argv.index('--off') + 1])
    if '--button' in argv:
        btn = int(argv[argv.index('--button') + 1], 16)
    if on <= 0 or off < 0 or frames <= 0:
        print('frames/on must be positive, off non-negative')
        return 2

    period = on + off
    pressed = 0xFFFF & ~btn        # active-low: clear the bit to press it
    masks = [pressed if (i % period) < on else 0xFFFF for i in range(frames)]
    with open(out, 'wb') as f:
        f.write(struct.pack('<%dH' % frames, *masks))

    npress = sum(1 for m in masks if m != 0xFFFF)
    print('wrote %s: %d frames, button 0x%04X (mask 0x%04X when pressed), %d on / %d off'
          % (out, frames, btn, pressed, on, off))
    print('  %d pressed frames, %d idle (%.1f%% duty)' % (npress, frames - npress, 100.0 * npress / frames))
    print('  replay with: PSXPORT_PAD_REPLAY=%s' % out)
    # Stated so a silent mismatch is impossible to miss: the port reads pad frames, and a replay
    # shorter than the run simply runs out — after which live/forced input applies again.
    print('  NOTE: past frame %d the replay is exhausted and input reverts to live/forced.' % frames)
    return 0


if __name__ == '__main__':
    sys.exit(main())
