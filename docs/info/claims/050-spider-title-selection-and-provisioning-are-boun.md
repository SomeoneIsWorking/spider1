---
id: C050
kind: claim
status: holds
created: 2026-08-22
tags: 
depends: tools/title_catalog.py#TitleCatalog,tools/provision.py#provision_executable,game/core/executable_identity.cpp#verifyExecutable,game/core/spider_port.cpp#runPort,CMakeLists.txt
reconfirmed: 2026-08-22 19:58:25
verified_at: 2026-08-22 19:58:25
---

## Claim

Spider title selection and provisioning are bound to the disc's SYSTEM.CNF serial and the selected
executable's measured size/SHA-256 before Game construction, with SLUS_008.75 and SLUS_013.78
selecting distinct runtime targets. Filename agreement alone is rejected
as insufficient identity.

## Evidence

`tests/test_title_catalog.py`, `tests/test_provision.py`, and the executable-identity CTest cover both supported serials and refuse missing, ambiguous, unknown, mutated, and human-generic identities. Provisioning validates the executable before replacing any previously valid destination. Each title is a distinct CMake runtime target, and `runPort` verifies the selected bytes again before constructing `Game`.

## What would falsify it

A selected title accepts media or an executable whose SYSTEM.CNF/filename serial differs, accepts
renamed or mutated executable bytes, both targets collapse to one title identity, or the launcher
constructs Game before serial and byte authentication.

## Re-confirmed 2026-08-22 19:58:25

Final d2266f4b verification adds binary isolation: nm -C finds EnterElectroRuntime only in enter_electro_port and no Spider1Runtime/spiderman_install/legacy measured-config symbols; spiderman_port contains no EnterElectroRuntime/enter_electro registry. Both selection tests, real media mismatch refusal, and pre-Game executable mismatch refusal still pass.

## Working-tree verification 2026-08-24

The shipping `game/core/executable_identity.cpp` path now authenticates serial, measured file size,
PS-X EXE magic, and SHA-256 before runtime installation or `Game` construction. Its both-answer
CTest accepts an exact fixture and rejects a wrong serial, mutated bytes, and invalid magic. The two
real extracted caches independently match their manifests exactly: `SLUS_008.75` is 749568 bytes /
`d2270e35581ba083d9441166e9a45ead4f869ab07e890f9a512ad7ee4cc0b15b`; `SLUS_013.78` is 786432
bytes / `dbe6c3f32337fe0fa7085519c728a75abf5d007b45ea0ba58178bcf84b72908a`. Clang build and full
CTest pass 12/12 against clean framework 9c2e3f1c. Landing operator must run `claim confirm C050`
after committing so the dependency baseline includes the new verifier.

The direct frozen `bootstrap.py --prepare-only` route then freshly authenticated the real Spider-Man
disc/executable, regenerated MAIN plus all 30 runtime modules, and built only `spiderman_port` in the
compiler-keyed, test-disabled player tree. No title process was launched.
