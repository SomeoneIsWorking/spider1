#!/usr/bin/env python3
"""present_geometry.py — is the presented picture the right SHAPE?

WHY THIS EXISTS. Issue 0008 (the picture presented 1.6x too wide for a whole session) was found by
the USER asking "are these stretched wide?" — not by any check in this repo. It could not have been
found by one, and that is the point worth understanding before using this tool:

    EVERY existing instrument here is INVARIANT UNDER THE BUG.
    Non-black coverage %, distinct-colour count, mean brightness, per-tile richness — a uniform
    rescale of the picture changes NONE of them. ppm_look.py reported "real frame, 62.0% non-black,
    528 colours" on a frame stretched 1.6x, and every one of those numbers was correct.

So the gap was not a weak instrument, it was a MISSING DIMENSION: nothing measured geometry. This
measures geometry and nothing else.

WHAT IT DOES. Finds the picture's content band inside the sink (the non-black bounding box), reports
its aspect, and compares it to the aspect the picture SHOULD have. On PSX every horizontal mode
(256/320/368/512/640) scans into the same 4:3 screen area, so pixels are non-square and the correct
presented aspect is 4:3 REGARDLESS of framebuffer width — unless a widescreen mod is deliberately
active, which is what --expect is for.

    python3 tools/present_geometry.py scratch/screenshots/present_10000.ppm
    python3 tools/present_geometry.py shot.ppm --expect 16:9        # widescreen mod active

WHAT IT CANNOT DO, stated so its silence is never mistaken for a pass:
  * It measures the BAND OF NON-BLACK PIXELS, not the intended picture rectangle. A frame whose
    content genuinely does not reach its own edges (a fade, a letterboxed cutscene, a mostly-black
    scene) will measure smaller than the true picture and may report a false mismatch. It says so
    when the band looks suspicious rather than reporting a confident number.
  * It CANNOT distinguish "the picture is correctly 4:3" from "the picture is square and the game
    happens to be drawing a square thing". It is a geometry check on the frame, not a check that the
    frame's CONTENT is undistorted.
  * A fully-black frame has no measurable geometry at all. It REFUSES (exit 2) rather than returning
    an aspect, because "0x0 band" would otherwise compare unequal to everything and read as a
    detected bug.
"""
import sys


def read_ppm(path):
    d = open(path, 'rb').read()
    parts, i = [], 0
    while len(parts) < 4:
        while i < len(d) and d[i:i + 1].isspace():
            i += 1
        if i < len(d) and d[i:i + 1] == b'#':                 # comment line
            while i < len(d) and d[i:i + 1] != b'\n':
                i += 1
            continue
        j = i
        while j < len(d) and not d[j:j + 1].isspace():
            j += 1
        parts.append(d[i:j])
        i = j
    i += 1
    if parts[0] != b'P6':
        raise ValueError('%s is not a P6 PPM (magic %r)' % (path, parts[0]))
    w, h = int(parts[1]), int(parts[2])
    px = d[i:i + w * h * 3]
    if len(px) != w * h * 3:
        raise ValueError('%s is truncated: %d pixel bytes, expected %d' % (path, len(px), w * h * 3))
    return w, h, px


def content_band(w, h, px, thresh=0):
    """Bounding box of pixels brighter than `thresh`. Full scan — no subsampling, because a
    subsampled scan can miss a one-pixel border and silently shrink the band."""
    rows, cols = [], []
    for y in range(h):
        base = y * w * 3
        for x in range(w):
            o = base + x * 3
            if px[o] > thresh or px[o + 1] > thresh or px[o + 2] > thresh:
                rows.append(y)
                break
    for x in range(w):
        for y in range(h):
            o = (y * w + x) * 3
            if px[o] > thresh or px[o + 1] > thresh or px[o + 2] > thresh:
                cols.append(x)
                break
    if not rows or not cols:
        return None
    return cols[0], rows[0], cols[-1] - cols[0] + 1, rows[-1] - rows[0] + 1


def parse_aspect(s):
    if ':' in s:
        a, b = s.split(':', 1)
        return float(a) / float(b)
    return float(s)


def main():
    # Consume option VALUES properly. The first version filtered on a leading "--", which left the
    # value of --expect (e.g. "16:9") in the positional list, and the tool then tried to open it as
    # a PPM. It crashed loudly rather than silently, which is the only reason it was caught in the
    # same minute it was written — but an argument parser that mistakes a flag's value for an input
    # is exactly how a tool ends up measuring the wrong file and reporting it with confidence.
    expect, expect_s, tol = 4.0 / 3.0, '4:3', 0.02
    args, rest = [], list(sys.argv[1:])
    while rest:
        a = rest.pop(0)
        if a == '--expect':
            if not rest:
                print('--expect needs a value like 4:3 or 16:9'); return 2
            expect_s = rest.pop(0); expect = parse_aspect(expect_s)
        elif a == '--tol':
            if not rest:
                print('--tol needs a value like 0.02'); return 2
            tol = float(rest.pop(0))
        elif a.startswith('--'):
            print('unknown option %s' % a); return 2
        else:
            args.append(a)
    if not args:
        print(__doc__)
        return 2

    rc = 0
    for path in args:
        w, h, px = read_ppm(path)
        band = content_band(w, h, px)
        if band is None:
            # A black frame has NO geometry. Refusing is the whole point: returning 0x0 here would
            # compare unequal to the expected aspect and print a confident "STRETCHED" for a frame
            # that simply has no picture in it.
            print('%s: sink %dx%d — ENTIRELY BLACK, no measurable geometry. REFUSING to report an '
                  'aspect (this is not a pass and not a fail).' % (path, w, h))
            rc = max(rc, 2)
            continue
        bx, by, bw, bh = band
        got = bw / float(bh)
        ratio = got / expect
        verdict = 'OK' if abs(ratio - 1.0) <= tol else ('STRETCHED %.3fx WIDE' % ratio if ratio > 1
                                                        else 'SQUASHED %.3fx (too narrow)' % ratio)
        print('%s' % path)
        print('  sink        %dx%d' % (w, h))
        print('  content     x %d..%d (%d wide), y %d..%d (%d tall)'
              % (bx, bx + bw - 1, bw, by, by + bh - 1, bh))
        print('  aspect      %.3f:1   expected %s = %.3f:1' % (got, expect_s, expect))
        print('  verdict     %s' % verdict)
        # The band touching every edge means the picture fills the sink; bars mean it does not.
        # Reported explicitly because "big black bars" is what this bug looks like to a human, and
        # a reader who sees bars tends to assume they are deliberate letterboxing.
        bars = []
        if by > 0 or by + bh < h:
            bars.append('top/bottom %d+%d px' % (by, h - (by + bh)))
        if bx > 0 or bx + bw < w:
            bars.append('left/right %d+%d px' % (bx, w - (bx + bw)))
        print('  bars        %s' % (', '.join(bars) if bars else 'none (fills the sink)'))
        if bw * bh < (w * h) // 20:
            print('  CAUTION     the content band covers <5%% of the sink — this may be a mostly-dark')
            print('              frame rather than a small picture, and the aspect above may not be')
            print('              measuring the intended picture rectangle at all.')
        if abs(ratio - 1.0) > tol:
            rc = max(rc, 1)
    return rc


if __name__ == '__main__':
    sys.exit(main())
