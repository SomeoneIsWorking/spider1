# 0027 — a mode whose object allocation returns zero writes into guest address 0x0B

**Status:** open, not fixed. This is a readability finding: the refactor that named these objects made
the defect visible, and fixing it would have been a behaviour change inside a change whose whole point
was that it changes nothing.

## What was found

Two of Spider-Man 1's retail modes obtain a pointer from a guest allocator and then use it
immediately, without asking whether the allocation succeeded. Both now hold that pointer in a NAMED
MEMBER, which is what made the missing check readable:

| mode | owner | the call | what happens on a null result |
|---|---|---|---|
| title menu | `Spider1MenuMode::begin` | `kUiAllocate` with `kMenuAllocateBytes` (1160) | see below |
| alternate (attract) | `Spider1AlternateMode::step` | `kAlternateObjectPointer` word | see below |

### The menu, before the allocation is checked

`spider1_menu_mode.cpp`, `begin()`:

```
object_ = core.r[2];                 // may be 0
if (object_ != 0) {                 // the ONLY null check, and it wraps the construct call
  ... kUiConstruct ...
  object_ = core.r[2];
}
core.r[4] = object_;                 // unguarded
spider1CallGuest(core, kUiBind, ...);
core.r[4] = object_;                 // unguarded
spider1CallGuest(core, kUiBind, ...);
core.mem_w8(object_ + kUnattributedMenuObjectByte0B, 0);    // 0x0000000B on a null object
core.mem_w8(object_ + kUnattributedMenuObjectByte18, 1);    // 0x00000012 on a null object
```

So a failed allocation still runs two `kUiBind` calls with a zero object, and still writes two bytes at
guest addresses `0x0B` and `0x12`. Those are main-RAM addresses, so the writes are silent rather than
faulting — which is why this has not been seen.

### The alternate mode, with no check at all

`spider1_alternate_mode.cpp`, `step()`:

```
const uint32_t object = core.mem_r32(spider1::alternateObjectPointer);
if (needsInit_) {
  core.mem_w8(object + kUnattributedAlternateObjectByte0E, flag_ ? 1 : 0);   // 0x0E on a null object
  core.r[4] = object;
  core.r[5] = 0;
  spider1CallGuest(core, alternateBegin, ...);
}
```

`kAlternateObjectPointer` (`0x800B49A0`) is a guest word the title's own code sets before entering the
alternate mode, so a zero there means the route was taken without the object ever being published. The
mode then writes `0x0E` and calls `alternateBegin(0)`.

## Why it was not fixed here

The refactor's contract was behaviour preservation, and every one of these code paths is behaviour. A
"fix" here would have to decide what a null object MEANS — refuse and return to the outer cycle, or
present a frame with no object, or something else — and that decision needs the retail behaviour, which
means reading the guest's own allocator contract out of the image. That is RE, not tidying.

## What would unblock it

The guest allocator's failure contract: does `kUiAllocate` returning 0 mean "no room" (and the retail
caller then skips the binds) or "the allocation is optional and the caller continues anyway"? The
answer decides whether the port should skip the binds, skip the whole mode, or leave the writes. Until
that is read out of the image, any change here is a guess, and a guess in a mode that currently runs is
worse than the defect being visible.

## Evidence

`spider1_menu_mode.cpp` `begin()`, `spider1_alternate_mode.cpp` `step()` — both are the extracted
owners, and both are reached by `Spider1ModeDriver`'s `AwaitMenuReady` and `AlternateFrame` states.
`docs/project-state.md` S018 records that neither the menu nor the alternate mode has been reached by an
authenticated run, which is why this has never fired.
