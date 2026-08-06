// scene_id.h — SceneName: the game's own scene identity, read from the level name it keeps in RAM.
//
// WHY THIS EXISTS. A native renderer needs to know WHICH scene it is being asked to draw, so that a
// scene with no producer can fail fast naming itself instead of drawing something plausible. Tomba!2
// keys on its scheduler's stage pointer + substate selectors; this game has no visible task table
// (docs/re-frontier.md RE-13), and the guest MODULE REGISTRY — the only other candidate — was
// MEASURED useless as a discriminator: 8 load/evict events over a 13757-present run, 94.3% of it on
// one constant resident set spanning the attract fly-through AND live gameplay (claim C026, issue
// 0011).
//
// The identity the game actually uses is its own CURRENT LEVEL NAME. It is a short ASCII string in
// main RAM, and the engine folds it into a small integer with FUN_8005A734 — the same integer the
// per-frame state machine FUN_80062CE0 (called twice per frame from the render walk FUN_8002BD5C)
// switches on, over the constants 0x100..0x105, 0x201, 0x202, 0x301, 0x302, 0x401, 0x501..0x503,
// 0x505..0x508, 0x604, 0x701, 0x702, 0x704, 0x803. So this is not an invented classifier: it is the
// engine's own, ported. (docs/re-frontier.md RE-23, claim C030.)
//
// COVERAGE, stated because it is the honest limit: this names the LEVEL, not the substate within it
// (front-end page, cutscene vs play). That is still RE-13.
#pragma once
#include <stdint.h>

class Core;   // external/psxport/runtime/recomp/core.h

// The 4 ASCII bytes at 0x800A568C that the engine encodes into a scene id, plus the encoding.
//
// PROVENANCE — every address here comes from the encoder's own instructions:
//
//   python3 tools/redump_ram.py
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8005A734 0x8005A7C0
//
//   8005A734  lui   $a1, 0x800a
//   8005A738  addiu $v1, $a1, 0x5688      ; the buffer PAIR base = 0x800A5688
//   8005A73C  lbu   $a0, 5($v1)           ; name[1] = 0x800A568D
//   8005A740  lbu   $v1, 4($v1)           ; name[0] = 0x800A568C   <- the string starts here
//   ...
//   8005A7A8  lbu   $v0, 7($v1)           ; name[3] = 0x800A568F
//
// The same 0x800A568C buffer is what main() (FUN_8002C354) hands to FUN_8005F1D4 / FUN_80018898 /
// FUN_80018800 on its case-3 path, next to the string literals at 0x800B4FD8 / 0x800B4FE0 — i.e. the
// mode switch writes it, which is why it is the current level name and not a scratch buffer.
class SceneName {
 public:
  // Guest address of name[0]. name[2] is read by nothing in the encoder and is carried only so the
  // diagnostic can print the whole 4-byte field the engine treats as one datum.
  static constexpr uint32_t kAddr = 0x800A568Cu;
  static constexpr int      kBytes = 4;

  // "nothing has been read yet" — all bytes zero, text "....", printable() false. A distinct state
  // from any real read, so a census can tell its first sample from a stale one.
  SceneName();
  explicit SceneName(Core* c);

  // The 4 bytes as a NUL-terminated C string, with every non-printable byte rendered '.' so a log
  // line can never be corrupted by whatever is really there. rawByte() is the unfiltered value.
  const char* text() const { return mText; }
  uint8_t     rawByte(int i) const { return mRaw[i]; }

  // True when all 4 bytes are printable ASCII. A false here is a REAL answer, not an error: it says
  // the mode switch has not written a level name yet (boot/FMV), and the code below still encodes
  // whatever is there — exactly as the guest's own encoder would.
  bool printable() const { return mPrintable; }

  // FUN_8005A734, ported. Returns (level << 8) | sub, the value FUN_80062CE0 switches on.
  //
  //   name[0] 'd' or 'D'                 -> level = 0x99   (a distinct scheme; see RE-23)
  //   else name[1] in '0'..'9'           -> level = name[1] - '0'          (0..9)
  //   else name[1] in 'A'..'Z'           -> level = name[1] - 0x31         (0x10..0x29)
  //   else name[1] in 'a'..'z'           -> level = name[1] - 0x51         (0x10..0x29, same range)
  //   else                                  level = name[1]                (raw byte, unfolded)
  //   sub = name[3] - '0', in WRAPPING 32-bit arithmetic exactly as the guest's `addiu` does
  //
  // The wrap is reproduced rather than clamped: a name[3] below '0' really does OR high bits into
  // the result on hardware, and a "tidied" version here would disagree with the state machine the
  // value is compared against.
  uint32_t code() const;

  bool sameAs(const SceneName& o) const;

 private:
  uint8_t mRaw[kBytes];
  char    mText[kBytes + 1];
  bool    mPrintable;
};
