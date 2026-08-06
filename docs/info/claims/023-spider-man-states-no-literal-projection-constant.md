---
id: C023
kind: claim
status: holds
created: 2026-08-06
tags: projection,gte,camera,re-17,native-renderer,512x240
depends: game/core/game_config.cpp, game/core/diag_overrides.cpp
---

## Claim

Spider-Man states NO literal projection constants: the port must RECORD (OFX, OFY, H) at libgte SetGeomOffset 0x8008BF24 / SetGeomScreen 0x8008BF14, and on the boot/front-end viewport those are OFX=256, OFY=120, H=276 — not the stock 160/120/350 and not Tomba!2's values.

## Evidence

The two leaves were located by the instruction that marks them: tools/ghidra_query.py scan ctc2 (I019) walked 130,588 disassembled instructions across 1,564 functions, found 1,404 ctc2 sites, exactly 10 touching cop2 control regs 24/25/26. 0x8008BF14 = {ctc2 $a0,$26; jr $ra; nop}; 0x8008BF24 = {sll $a0,$a0,16; sll $a1,$a1,16; ctc2 $a0,$24; ctc2 $a1,$25; jr $ra; nop}. Ghidra had disassembled only 24.9% of the 2 MB image, so a raw word scan of ALL 524,288 words of ram.bin for every ctc2 rX,CR24/25/26 encoding and for jal to each leaf returned the SAME 10 sites and the SAME one caller each. That one caller, FUN_80075D0C, passes COMPUTED viewport fields, not constants: vp[8]=(vp[2]+vp[0])>>1, vp[9]=(vp[3]+vp[1])>>1, vp[7]=((((vp[0]-vp[2])>>1)<<12)/vp[6]<<12)/*(int*)(gp+0x1140); the caller reloads them as lhu 0xE/0x10/0x12($s4) at 0x8007617C/0x80076188/0x8007618C. On a real boot with the real disc, PSXPORT_DEBUG=geomwatch (I018) reports valid=1 OFX=256 OFY=120 H=276 and 'GATE PASSED: requireGeom() returned OFX=256 OFY=120 H=276 without aborting' (scratch/logs/g7/run_gate.log). NEGATIVE CONTROL, same instrument, same run conditions, only the two GameConfig addresses zeroed: valid=0 OFX=0 OFY=0 H=0 and requireGeom aborts with 'SetGeomOffset (OFX/OFY) NEVER RAN' (scratch/logs/g7/run_gate_control.log). OFX=256 = 512/2, the centre of this game's 512-wide framebuffer. NOT A TAP: nothing reads CR24/25/26 back out of the GTE; the value is recorded where the game states it.

## What would falsify it

Observing SetGeomOffset/SetGeomScreen called with a DIFFERENT triple in a viewport this run never reached (gameplay, a cutscene, a split view) would falsify the '256/120/276' half — and is EXPECTED, because H is derived per viewport. It would NOT falsify the mechanism half. What WOULD falsify the mechanism: finding a write to CR24/25/26 that does not pass through these two leaves and does not restore a value they set. One such site exists and was checked — FUN_8007C2AC zeroes CR24/25 at 0x8007C0B4/B8 and restores them at 0x8007C268/6C from *(*0x800B5918+0x10), which is the SAME vp[8]/vp[9] (0x800B5918 = gp+0x1124, gp=0x800B47F4, written by FUN_80075D0C). If a future run shows ProjParams disagreeing with the GTE's CR24/25, that inference is wrong.
