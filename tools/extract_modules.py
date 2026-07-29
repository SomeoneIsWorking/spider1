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

THE LOAD BASE IS A MEASUREMENT, AND THE PORT CHECKS IT. The module's address comes from the game's
own allocator, not from a fixed slot, so it cannot be read out of the binary — it is observed, and
observed to be identical across runs. That makes it exactly the kind of constant this project
distrusts, so it is not merely written down: the port verifies at load time that the module landed
where the substrate was built for and aborts loudly if it did not (game/core/module_loader.cpp).
A silently-wrong base would mean executing one module's recompiled code against another's data.
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Modules to extract, and the guest address each is loaded at. See the module note above: these
# bases are MEASURED from a real run and re-checked at runtime by the port, never assumed.
MODULES = {
    "shell": 0x8014D5AC,
}


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

        for stem, base in MODULES.items():
            img = bytearray(grab(f"{stem}.bin"))
            counts = relocate(img, grab(f"{stem}.rel"), base)
            dest = os.path.join(out_dir, f"{stem}.bin")
            with open(dest, "wb") as f:
                f.write(img)
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
