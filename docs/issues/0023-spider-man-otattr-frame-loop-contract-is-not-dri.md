---
id: 23
title: Spider-Man OtAttr frame-loop contract is not driven
status: resolved
symptom: real 5150-frame product run logs millions of OtAttr table stamps at frame zero because Spider1FrameDriver never calls beginLogicFrame
tags: frame-loop,otattr,diagnostic,spiderman1
created: 2026-08-27
updated: 2026-08-28
---

A real-disc 5,150-frame witness against psxport 3c342ec3 exited 0 and reconciled every presentation fence, but producer_db_finish warned that 37,608,780 table stamps occurred without OtAttr::beginLogicFrame. Static inspection found Spider1FrameDriver::stepFrame sets game_.timing.logicFrame and owns every product logic step, but omits the required rsub OtAttr frame declaration. Add the call at that boundary, guard its placement with the native-frame ownership test, rebuild, then rerun the product after serialized runtime authorization.

### Note (2026-08-27)
Root cause fixed in the working tree: Spider1FrameDriver::stepFrame now calls core.rsub.otAttr.beginLogicFrame(frame) immediately after publishing timing.logicFrame and before pad/guest services. The focused native-frame ownership regression requires exactly one call in that ordering. Isolated 3c342ec3 Clang rebuild and all 21 CTests pass. Keep investigating until a serialized real-product rerun proves reportFrameContract says SATISFIED and the prior warning is absent.

### Resolution (2026-08-28)

The exact-pin Clang product build against psxport 319d30b6 completed a serialized 120-frame real-disc
run. `producer_db_finish` reported the frame-loop contract `SATISFIED` at frame 119 after 92,365
pre-frame stamps; all 120 presentation-ledger entries reconciled and native crt0 returned cleanly.
This is the required opposite-answer runtime witness for the OtAttr instrument and resolves this issue.

The run did not establish Spider-Man gameplay: the finite boot route stopped after failing to locate
`/CINEMAS/TTSLOGO.STR;1`, before the outer dispatcher, and the old long-run gate therefore exited 1.
That later CD/movie/dispatcher boundary is separate from the OtAttr ownership defect resolved here.
Evidence is in `scratch/logs/gate-boot-20260828-004751.log`.
