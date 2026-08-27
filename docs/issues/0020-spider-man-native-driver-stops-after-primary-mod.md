---
id: 20
title: Spider-Man native driver stops after primary mode exit because outer mode dispatch remains guest-owned
status: resolved
symptom: native Spider-Man reaches the end of 0x8002C174 but cannot transition into 0x800604CC, 0x800160EC, or 0x8006F294
tags: frame-loop,mode-dispatch,spiderman1,re-22
created: 2026-08-27
updated: 2026-08-27
---

State items: S002, S004.

The first title-local Spider1FrameDriver owned only the finite 0x8002C174 primary iteration and its teardown. The retail 0x8002C354 switch above it carries persistent outer-mode locals and chooses the transition/menu/alternate functions, so discarding that continuation made every primary exit terminal even though the generated functions themselves remained available.

The required boundary is one Spider-Man 1 outer-mode owner with separately derived finite state for 0x800604CC, 0x800160EC, and 0x8006F294. Falling back to the non-returning guest main or copying Enter Electro addresses would reintroduce the ownership defect.

### Resolution (2026-08-27)
Root cause was that the first native driver stopped at FUN_8002C174 and discarded FUN_8002C354's persistent selector plus FUN_800604CC/FUN_800160EC/FUN_8006F294 continuation state. Spider1ModeDriver now owns the authenticated ten-route table and each loop as finite submitted/repeated/unpresented steps; generated supers remain compiled and Enter Electro stays isolated. Clang product link, selector transition test, focused runtime/static gates, format and tidy pass without launching. Runtime selector-parity evidence remains an explicit RE-22/S002 gap, not an unresolved ownership path.
