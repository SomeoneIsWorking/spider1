// cd_stream.cpp — drive a continuous CD read (XA / STR) from the point the guest asks for data.
//
// A stream is not a file read. A file read is finite and the port serves it in one call; a stream
// runs until the game stops it and expects sectors delivered as it consumes them.
//
// WHY HERE, measured rather than assumed — two other homes were tried and rejected:
//   * The port's field clock (vblank advance). Wrong: over a boot that reaches the movie the guest
//     makes only 13 VSync calls in total and stops calling it entirely once inside the streaming
//     loop, so a pump hung there never fires when it is needed.
//   * Nowhere at all, relying on the file-read burst. That burst drives a FINITE read to completion
//     and a stream has no end for it to reach; it ran away to its 65536-sector bound and wedged the
//     boot. It is now correctly restricted to games that have not taken over CdRead.
//
// The streaming loop DOES call StGetNext every iteration, which makes this the honest place: supply
// a sector at the moment the guest asks for one and finds none ready.
//
// WHAT THE PUMP ACTUALLY DRIVES, and why that is not a shortcut around the guest. `pumpStream`
// invokes the callback the guest registered through CdReadyCallback (GameConfig::cdReadyCbPtr,
// 0x800B3B18). During an STR stream that callback is 0x800860B4 -> FUN_80085000, libstr's own
// sector-arrival handler: it DMAs the sector into the ring, checks the STR magic, maintains the
// producer index and writes the wrap marker. So the port supplies the DRIVE'S PACING and the guest
// keeps every ring invariant, which is the correct division. Nothing here touches the ring.
#include "core.h"
#include "game.h"
#include "cfg.h"
#include "override_registry.h"
#include <lucent/log.h>

// StGetNext(&addr, &header) — 0x80086B10. Returns 0 when it hands back a ready ring slot, non-zero
// when none is ready. Read from its body: it takes the ring slot at DAT_800c1510 + index*0x20 when
// the slot's status short is 2, and marks it 4.
extern void gen_func_80086B10(Core*);

// The libstr sector ring's control block. Every one of these was read out of the ring's own three
// routines, decompiled with `tools/ghidra_query.py func 0x80086B10 0x800872AC 0x80085000`:
//
//   StGetNext  (0x80086B10)  slot i header at ringBase + i*0x20; its data at
//                            ringBase + slotCount*0x20 + i*0x7E0.
//   StFreeRing (0x800872AC)  zeroes header[3] (= chunks in this frame) slot statuses from the freed
//                            index and sets consIdx = index + that count.
//   producer   (0x80085000)  fills slot writeIdx, and when a new frame will not fit before the end
//                            of the ring writes status 1 (the wrap marker) and restarts at 0.
//
// A slot's status short is: 0 free, 1 wrap marker, 2 frame ready, 3 DMA in flight, 4 held by the
// consumer.
static constexpr uint32_t kRingBase       = 0x800C1510u;  // -> the slot-header array
static constexpr uint32_t kRingWriteIdx   = 0x800C1514u;  // producer's next slot
static constexpr uint32_t kRingFrameStart = 0x800C1518u;  // first slot of the frame being filled
static constexpr uint32_t kRingConsIdx    = 0x800C151Cu;  // consumer's next slot
static constexpr uint32_t kRingSlotCount  = 0x800C1520u;  // slots in the ring
static constexpr uint32_t kRingSlotStride = 0x20u;        // bytes per slot header

static void spiderman_stgetnext(Core* c) {
  gen_func_80086B10(c);
  // Non-zero means "no sector ready" — exactly when the drive owes the guest one. Pump a sector and
  // answer again. EXACTLY ONE retry: if the stream still has nothing after being pumped, "not ready"
  // is the honest answer and the guest is entitled to keep spinning. Looping here until something
  // appeared would mask a genuinely dry stream and hang inside an override instead of in the game.
  if (c->r[2] != 0) {
    c->game->cd.pumpStream(c, 1);
    gen_func_80086B10(c);
  }

  // `PSXPORT_DEBUG=ring` — the sector ring's actual state when the guest finds nothing ready.
  //
  // Two opposite faults look identical from outside: the PRODUCER never marking slots ready, and the
  // CONSUMER reading the wrong slot. They need opposite fixes, and guessing between them has already
  // cost several attempts. The slot status words settle it.
  //
  // The whole ring is printed, sized by the guest's own slot count. A previous version printed a
  // fixed twelve slots and never read the count, so "the consumer index has run past the end" could
  // only be inferred from consIdx > writeIdx — the ring's actual size was never on screen.
  //
  // Decimated, and the guard is around the ring WALK (a dozen guest reads inside a spin loop that
  // runs millions of times), not around the log call.
  if (c->r[2] != 0 && lucent::channel_on("ring")) {
    static unsigned n = 0;
    if ((n++ % 200000) == 0) {
      const uint32_t base  = c->mem_r32(kRingBase);
      const uint32_t slots = c->mem_r32(kRingSlotCount);
      lucent::Line status;
      for (uint32_t i = 0; i < slots && i < 64; i++)
        status.add("{} ", (int)(int16_t)c->mem_r16(base + i * kRingSlotStride));
      lucent::debug("ring", "base=0x{:08X} slots={} frameStart={} cons={} writeIdx={} | status: {}",
                    base, slots, c->mem_r32(kRingFrameStart), c->mem_r32(kRingConsIdx),
                    c->mem_r32(kRingWriteIdx), status.view());
    }
  }
}

void spiderman_install_cd_stream(Game* g) {
  engine_set_override_main(0x80086B10u, spiderman_stgetnext, gen_func_80086B10);
  lucent::info("cd", "continuous-read pump installed on StGetNext (0x80086B10)");
  (void)g;
}
