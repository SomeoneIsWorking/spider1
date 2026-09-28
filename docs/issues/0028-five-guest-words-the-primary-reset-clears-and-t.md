# 0028 — five guest words the primary reset clears, and two display-buffer bytes, are still unattributed

**Status:** open. This is a RE gap, not a defect: nothing is known to be wrong, and nothing is claimed
to be right.

## What is now named, and how honestly

The readability pass moved the title's guest facts into one owner, `spider1_guest_layout.h`, and every
one of them is named. Most names assert only what the executable or the driving code already shows. For
eight facts the driving code shows what it DOES and not what it IS, so those names say so:

| name | address | what IS known | what is NOT |
|---|---|---|---|
| `kUnattributedWord4F38` | `0x800B4F38` | cleared once per primary initialization, immediately before the primary reset | whether the retail body reads it, writes a counter into it, or treats it as a flag |
| `kUnattributedWord5690` | `0x800B5690` | same, cleared immediately after the primary reset | as above |
| `kUnattributedWord5694` | `0x800B5694` | same | as above |
| `kUnattributedWord4FEC` | `0x800B4FEC` | same | as above |
| `kUnattributedWord5004` | `0x800B5004` | same | as above |
| `displayBufferTableByte18` | `0x8009A6E4` + 24 | cleared before a mode's first field, set after its last | the record's layout — this is a static-data address, not a libgpu environment block |
| `displayBufferTableByte90` | `0x8009A6E4` + 144 | written with the OPPOSITE value at the same two points, so the pair is a swap | as above |
| `kUnattributedMenuConstructWord3` | menu block + 8 | written with the constant 16 once | the field's name |
| `kUnattributedAlternateObjectByte0E` | alternate object + 14 | set from the mode's flag, tested for 1 after release, drives the repeat probe | the field's name |
| `kUnattributedCrt0Register23` | `r[23] = 0x80090000` | restored by the boot prefix before the mode driver starts | what register `$s7` is for in this title |
| `padStateByte30/31/E0/E1` | `kPadState` + 48/49/224/225 | 48 and 224 are tested non-zero to request an exit and 49 and 225 are cleared by the same branch, so each pair is a latched press and its acknowledgement | which buttons they are, and the buffer's own field names |
| `kUnattributedMenuObjectByte0B/18` | menu object + 11/24 | written as a 0/1 pair when the menu is built, which is the shape of a visibility pair | the field names |
| `drawBufferOtLengthOffset` | draw buffer + 112 | read out of the current draw buffer and passed as the ordering-table relink's length argument | the field's own name in the draw-buffer record |
| `uiTableQuadAssetEntry` / `BindEntryFirst` / `BindEntrySecond` | `kUiTable` + 416 / 808 / 820 | three whole words read from the UI table and passed to a UI call | what each entry DESCRIBES |
| `levelVisitCounterFirstKey` / `SecondKey` | `levelName` + 81 / 82 | one byte per lookup key, bumped with saturation, one set before the outer cycle and one before the primary | why there are two, and what the game does with the counts |
| `resourceFindFirstId` / `SecondId` | 4128 / 756 | the two ids the ResourcePrimary route looks up, in order | the resource id space |

The rule the names follow is the one the operator's directive states: **an honest "we do not know what
this is" name is what makes a black box readable, and it is the first step to finding out.** Each of
these now says, in the header, exactly what the port does know and exactly what it does not, so the next
session does not have to re-derive the same facts to know which questions are still open.

## Two DUPLICATE NAMES FOR ONE GUEST FUNCTION, found while doing it

Both were the same address under two names in two different files, which is the shape a reader cannot
see and a compiler cannot catch:

* `kPadService` (`spider1_frame_driver.cpp`) and `kPadRead` (`spider1_mode_driver.cpp`) were both
  `0x8006B514`. One is now `spider1::padRead`, in the one owner header.
* `kAlternateUpdate` was `0x80016CA4`, which is exactly `kUiUpdate`. So the alternate mode's per-field
  object update is the MENU's update call — one guest function, two modes. It is now one name, and the
  alternate mode says so in a comment, which is a small piece of recovered behaviour that was
  previously invisible because the two names implied two things.

## What would close it

A Ghidra decompilation of the primary reset body around `0x8006AF28` and of the table at `0x8009A6E4`.
Neither needs a new instrument: `tools/ghidra_query.py` and `tools/ghidra_export.py` are already in this
repository, and the addresses above are already written down, which is the half that was missing before.

## Evidence

`titles/spiderman1/spider1_guest_layout.h` — the named list, with each unattributed fact's comment
stating what is and is not known. `docs/re-frontier.md` owns the order; this issue owns the facts.
