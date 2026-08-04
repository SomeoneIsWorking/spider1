#!/usr/bin/env python3
"""extract_modules.py — pull the game's runtime-loaded code modules out of CD.WAD and relocate them.

WHY THIS EXISTS. SLUS_008.75 is not the whole game. Beyond the boot executable's text
(0x80010000 .. 0x800C6800) the game loads further CODE at runtime out of the packed archive CD.WAD,
as `<name>.bin` + `<name>.rel` pairs. The static recompiler only ever saw the executable, so the
first call into such a module aborted with a recomp MISS. The modules are ordinary MIPS, so they can
be recompiled like any other input — but only after their relocations are applied, because on disc
they are position-independent and every absolute address in them is a placeholder.

This script is the offline half of the game's own loader. It reads no game data into the repo: the
disc is the user's, the extracted bytes land in the gitignored scratch/, and nothing here is
committed.

THE LOADER, and where each rule below comes from (see docs/re-frontier.md RE-09):

  FUN_8001B990(name, kind)   the module loader
      loads "<name>.bin" -> a heap allocation, loads "<name>.rel",
      FUN_8001BF58(rel, bin) applies the relocations in place,
      frees the .rel, then calls the module's base address as its entry point.

  FUN_80064B3C(name)         the CD.HED index lookup
      CD.HED is a flat table of entries, each: a NUL-terminated name, then the read cursor advanced
      to ((nul_pos + 4) & ~3), then two little-endian u32 — the byte OFFSET into CD.WAD and the
      SIZE. Name comparison is case-insensitive. The size the loader allocates is rounded up to a
      2048-byte multiple, which is also why each entry's offset is its predecessor's offset+size
      rounded the same way.

  FUN_8001BF58(rel, base)    the relocator — a flat stream of u32 words, 0xFFFFFFFF terminating.
      Each word packs a type in its low 2 bits and a byte offset in the rest (offset = w & ~3):
        0  R_MIPS_32   *p += base
        1  R_MIPS_HI16 the NEXT word is the addend A;
                       *p = (*p & 0xFFFF0000) | (((A + base + 0x8000) >> 16) & 0xFFFF)
                       (the +0x8000 is the usual carry compensation for a following signed LO16)
        2  R_MIPS_LO16 *p = (*p & 0xFFFF0000) | ((*p + base) & 0xFFFF)
        3  R_MIPS_26   *p = (*p & 0xFC000000) | ((((*p & 0x3FFFFFF) * 4 + base) >> 2) & 0x3FFFFFF)
      Only type 1 consumes a second word.

VERIFIED, not inferred: relocating shell.bin offline at its observed load base reproduces the
running game's memory image byte-for-byte over all 112912 bytes, with the .rel stream consuming
exactly its 8416 bytes and terminating cleanly. See docs/info/claims.md CLAIM-08.

THERE IS NO LOAD ADDRESS TO RECORD, and that is the point. The game allocates each module from
its own heap, so where it lands depends on load order and differs per module and per playthrough,
and several modules are resident at once. The images written here are therefore relocated to an
arbitrary LINK base and recompiled BASE-RELATIVE (external/psxport/tools/recomp/emit.py); the port
observes the real base at load time (game/core/module_loader.cpp) and the framework's live registry
routes dispatch by it. Nothing downstream may bake a module address in.
"""
import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The LINK base every module is relocated to for recompilation. It is NOT where a module runs: the
# game's allocator picks that at load time and it differs per module and per playthrough. The
# recompiled code is emitted BASE-RELATIVE (external/psxport/tools/recomp/emit.py, ModuleReloc) and
# the runtime adds the difference, so this value only has to be a fixed, valid guest address that the
# whole build agrees on — it must equal overlay_base_patterns in game/recomp_seeds.json.
LINK_BASE = 0x800C65EC

# Every module pair in CD.HED. Discovered from the index rather than listed by hand, so a module that
# exists on disc cannot be silently skipped.
def modules_from_index(index):
    stems = {n[:-4] for n in index if n.endswith(".bin")} & {n[:-4] for n in index if n.endswith(".rel")}
    return {s: LINK_BASE for s in sorted(stems)}


def parse_index(hed: bytes):
    """CD.HED -> {lowercased name: (offset, size)}. Mirrors FUN_80064B3C's walk exactly."""
    out, i = {}, 0
    while i < len(hed):
        nul = hed.find(b"\0", i)
        if nul < 0:
            break
        name = hed[i:nul].decode("ascii", "replace")
        if not name:
            break                       # a zero-length name is the end of the table
        data = (nul + 4) & ~3
        if data + 8 > len(hed):
            break
        off, size = struct.unpack_from("<II", hed, data)
        out[name.lower()] = (off, size)
        i = data + 8
    return out


def relocate(img: bytearray, rel: bytes, base: int) -> dict:
    """Apply FUN_8001BF58's relocation stream to `img` in place. Returns a per-type count.

    Raises on a malformed stream rather than stopping early: a truncated relocation table would
    leave some absolute addresses as unrelocated placeholders, and code that jumps through one of
    those fails far away from the cause.
    """
    counts = {0: 0, 1: 0, 2: 0, 3: 0}
    i = 0
    while True:
        if i + 4 > len(rel):
            raise ValueError("relocation stream ran off the end without a 0xFFFFFFFF terminator")
        (w,) = struct.unpack_from("<I", rel, i)
        i += 4
        if w == 0xFFFFFFFF:
            break
        t, off = w & 3, w & ~3
        if off + 4 > len(img):
            raise ValueError(f"relocation at +0x{off:X} is past the {len(img)}-byte module")
        (cur,) = struct.unpack_from("<I", img, off)
        if t == 0:
            v = cur + base
        elif t == 1:
            if i + 4 > len(rel):
                raise ValueError("HI16 relocation is missing its addend word")
            (addend,) = struct.unpack_from("<I", rel, i)
            i += 4
            v = (cur & 0xFFFF0000) | (((addend + base + 0x8000) >> 16) & 0xFFFF)
        elif t == 2:
            v = (cur & 0xFFFF0000) | ((cur + base) & 0xFFFF)
        else:
            v = (cur & 0xFC000000) | (((((cur & 0x3FFFFFF) * 4 + base) >> 2)) & 0x3FFFFFF)
        struct.pack_into("<I", img, off, v & 0xFFFFFFFF)
        counts[t] += 1
    return counts


def hi16_offsets(rel: bytes):
    """The byte offsets at which the .rel stream relocates a HI16 — the ONLY thing the recompiler
    needs in order to emit a module base-relative.

    WHY ONLY HI16. A module's base-dependent words are of four kinds, and three of them need nothing
    from this table. The R_MIPS_32 data words are relocated by the GAME in its own RAM and the port
    reads them from there. The R_MIPS_26 jump targets and the low halves are handled by the emitter
    from the instruction stream itself: a jump target is base-dependent exactly when it lands inside
    the module (an address test, not a table lookup — and unlike this table that test also covers
    PC-relative BRANCH targets, which no relocation table ever names), and a low half needs no
    adjustment at all once the `lui` feeding it carries the module delta. What the emitter cannot
    derive is WHICH `lui` builds a module-relative address rather than a resident-executable one.
    That is what this returns.
    """
    out, i = [], 0
    while True:
        if i + 4 > len(rel):
            raise ValueError("relocation stream ran off the end without a 0xFFFFFFFF terminator")
        (w,) = struct.unpack_from("<I", rel, i)
        i += 4
        if w == 0xFFFFFFFF:
            break
        t = w & 3
        if t == 1:
            out.append(w & ~3)
            i += 4                      # HI16 is the only type that carries an addend word
    return out


def extract(hed_path: str, wad_path: str, out_dir: str, quiet: bool = False) -> int:
    index = parse_index(open(hed_path, "rb").read())
    os.makedirs(out_dir, exist_ok=True)
    n = 0
    with open(wad_path, "rb") as wad:
        def grab(name):
            key = name.lower()
            if key not in index:
                raise KeyError(f"{name} is not in CD.HED ({len(index)} entries) — wrong disc?")
            off, size = index[key]
            wad.seek(off)
            data = wad.read(size)
            if len(data) != size:
                raise ValueError(f"{name}: CD.WAD ended after {len(data)} of {size} bytes")
            return data

        for stem, base in modules_from_index(index).items():
            img = bytearray(grab(f"{stem}.bin"))
            rel_bytes = grab(f"{stem}.rel")
            counts = relocate(img, rel_bytes, base)
            dest = os.path.join(out_dir, f"{stem}.bin")
            with open(dest, "wb") as f:
                f.write(img)
            # The relocation SIDECAR, read by the recompiler. The framework knows nothing about this
            # console's relocation format — the game hands it plain offsets. `counts` travels with it
            # so a disagreement between what was applied here and what the emitter consumed is
            # visible instead of silent.
            with open(os.path.join(out_dir, f"{stem}.reloc.json"), "w") as f:
                json.dump({"size": len(img), "link_base": base,
                           "hi16": hi16_offsets(rel_bytes),
                           "counts": {"w32": counts[0], "hi16": counts[1],
                                      "lo16": counts[2], "j26": counts[3]}}, f)
            n += 1
            if not quiet:
                print(f"[modules] {stem}: {len(img)} bytes @ 0x{base:08X} "
                      f"(reloc 32:{counts[0]} hi16:{counts[1]} lo16:{counts[2]} j26:{counts[3]})")
    return n


def main():
    if len(sys.argv) < 4:
        sys.exit("usage: extract_modules.py <CD.HED> <CD.WAD> <out-dir>")
    try:
        extract(sys.argv[1], sys.argv[2], sys.argv[3])
    except (KeyError, ValueError) as e:
        sys.exit(f"[modules] error: {e}")


if __name__ == "__main__":
    main()
