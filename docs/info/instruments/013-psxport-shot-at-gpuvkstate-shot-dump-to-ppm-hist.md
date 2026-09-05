---
id: I013
kind: instrument
status: DISTRUSTED
created: 2026-08-05
distrusted_on: 2026-08-05
---

## Instrument

PSXPORT_SHOT_AT / GpuVkState::shot() -> dump_to() PPM histogram (external/psxport/runtime/psx/gpu_vk.cpp:1202 and :1171)

## Validated by

It IS validated as a VRAM-content instrument and it does show the other answer there: the same histogram read 0.00% non-black / 1 colour on every intro shot before the RE-07 fixes and 99.95% / 11395 colours after (C020). What it CANNOT do is what it was read as doing. dump_to() reads back the guest VRAM texture s_vram_tex and decodes the display region ITSELF; it never samples the swapchain, so it is blind to everything between VRAM and the display — present mode, swapchain acquire, letterbox, fade, window state. It also cannot distinguish 'the guest drew nothing' from 'the window shows nothing'. Scope it to: what is in guest VRAM at present index N. Never cite it for what the player sees

## Known failure modes

(none recorded yet)

## DISTRUSTED 2026-08-05

DISTRUSTED FOR THE QUESTION IT KEEPS BEING ASKED — 'what does the player see?' — not for what it measures. On 2026-08-05 it certified 99.95% non-black at f120 while the USER, watching the window, saw a black screen, and the numbers were true: the guest VRAM did contain the Activision logo in the HEADLESS leg it was run in. Windowed, the same instrument reads 0.00% / 1 colour — also true, and also not about the window. The defect is that a VRAM readback was quoted as a display measurement for a whole session, and nothing in its output says it never touched the swapchain. TRUST IT FOR: guest VRAM content at a present index, with a stated headless/windowed leg. DO NOT TRUST IT FOR: whether anything reached the display, whether present/composite/fade work, or whether the window is alive. THE MISSING INSTRUMENT: nothing in this port samples the SWAPCHAIN. Until one exists, every 'the picture is correct' result is a claim about VRAM. See issue 0005, C019 (falsified), C020 (the scoped re-issue)

> Every result this instrument produced is suspect until it is re-validated.
