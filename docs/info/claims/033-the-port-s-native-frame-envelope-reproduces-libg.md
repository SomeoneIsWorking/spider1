---
id: C033
kind: claim
status: holds
created: 2026-08-06
tags: render,producer,re-21,envelope
depends: game/render/gpu_env.cpp, game/render/frame_envelope.cpp
---

## Claim

The port's native frame envelope reproduces libgpu's PutDrawEnv/PutDispEnv words EXACTLY — 2560 word-for-word comparisons on a real boot, 0 mismatches

## Evidence

MEASURED 2026-08-06, headless, PSXPORT_NOPACE=1, reference leg, PSXPORT_DEBUG=envcheck (scratch/prod1/logs/envcheck2.log). FrameEnvelope::verifyAgainstGuest recomputes the 6 DRAWENV state words (GP0 E3/E4/E5/E1/E2/E6) plus the 3 background-clear words from the SAME DRAWENV block the guest hands PutDrawEnv, and compares them against the DR_ENV packet the guest's own PutDrawEnv left at DRAWENV+0x1C, INCLUDING the packet's word-count byte. Result: checked=2560 mismatch=0, sustained across the boot-init frame, the dem1 attract screen and l1a1 gameplay (3 scene identities, both double-buffer contexts). ORDERING MATTERS AND WAS GOT WRONG FIRST: run in the seam's pre-super-call pass the same check reported 6/1024 mismatches, because the packet in guest RAM is then two frames stale; moving it after the super-call is what makes it a comparison of this frame's words. THE COMPARATOR IS PROVEN TO FIRE ON THE OTHER CLASS: with ONE bit perturbed in DrawEnv::drawOffsetWord (mOfsX -> mOfsX+1), same binary otherwise, mismatch tracked checked exactly — 512/512, 1024/1024, 1536/1536 — and the per-word grid named E5 as the differing word. The perturbation was reverted and the clean run re-measured. The port derives every word from the game's own submission INPUTS (the DRAWENV/DISPENV structs plus libgpu's VRAM-extent and video-standard bytes) and copies no word from the guest's packet; the RE is from Ghidra headless on FUN_80081F40 / FUN_80082770 / FUN_80082000 / FUN_80082A00 / FUN_80082A98 / FUN_80082B30 / FUN_800829E0 / FUN_80082B4C. INDEPENDENT CORROBORATION of the DISPENV half: displayModeWord() computes GP1(08)=0x08000002 from the DISPENV, and the framework's own GPU model logs the identical word from the guest's boot.

## What would falsify it

a mismatch on any DRAWENV this game has not yet exercised — every observed DRAWENV had clip.x=0, ofs.x=0, a 64-aligned width and isbg=1, so the GP0(60) non-aligned clear branch and every non-zero texture window are UNTESTED CODE despite being ported; and PutDispEnv's GP1(06) is not emitted at all
