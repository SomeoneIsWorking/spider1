---
id: C051
kind: claim
status: holds
created: 2026-08-22
tags: 
depends: titles/spiderman2/enter_electro_runtime.cpp#EnterElectroRuntime::bootInit, titles/spiderman2/enter_electro_runtime.h#EnterElectroRuntime, tests/enter_electro_runtime_test.cpp#main
---

## Claim

Enter Electro has a direct derived runtime with no Spider-Man 1 GameConfig, GameHooks, or context, and its verified live boundary is the first game-owned call at 0x80031F54 after crt0.

## Evidence

On clean psxport d2266f4b, enter_electro_runtime CTest proves null legacy config/hooks/context and the measured GuestProgramImage. Real USA SLUS_013.78 boot reports crt0 audit 10 AGREE, 0 DISAGREE, 0 unresolved, applies heap 0x800CF0E0/0x728F1C, then emits the EE-02 refusal at gameMain 0x80031F54 with no recomp-MISS (scratch/logs/enter-electro-boundary.log).

## What would falsify it

EnterElectroRuntime binds any legacy compatibility view, the live crt0 audit disagrees or becomes unresolved, execution passes 0x80031F54 without RE evidence, or a render/gameplay claim is made before EE-02 lands.

## Working-tree verification 2026-08-24

Against clean psxport `9c2e3f1c`, the focused runtime CTest again proved null legacy
config/hooks/context, the complete measured `GuestProgramImage`, and a fail-closed unmeasured
picture policy; the full Clang suite passed 12/12. Binary isolation found Enter Electro's runtime and
installer in `enter_electro_port` and no Spider1Runtime, `spiderman_install_*`, or
`legacy::measuredConfig`. No new live title run was made, so the earlier `d2266f4b` log remains the
latest evidence that execution itself reaches the EE-02 refusal.
