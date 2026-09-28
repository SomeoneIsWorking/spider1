#include "spider1_cd_initialization.h"

#include "spider1_platform_facts.h"

#include "core.h"

#include <lucent/log.h>

namespace spider {
namespace {

// The mask bit the retail CdInit body reaches, and NOT the reason the CD channel is serviced.
//
// MEASURED 2026-09-27, `scratch/dem1/probe_gate.log`: the controller queued a sector and latched
// IRQ2, and the guest's registered CD interrupt element was never invoked, because `Hle::irqPoll`
// delivers `i_stat & i_mask` and 0x004 & 0x009 is 0. The title added the arm (0x009 -> 0x00D), and
// the gate opened.
//
// MEASURED 2026-09-28, `docs/issues/0026`: **opening the gate was not the fix, and the reasoning
// this comment used to give was wrong on two counts. Both corrections are from the executable's own
// bytes, and the second is the one that matters:**
//
// 1. 0x8008B86C is NOT a B-vector thunk. It is an indirect call through this title's own tables:
//        0x8008B86C  lui $v0,0x800B ; lw $v0,0x390C($v0)   ; $v0 = [0x800B390C] = 0x800B38EC
//        0x8008B87C  lw $v0,8($v0)                          ; $v0 = [0x800B38EC+8] = 0x8008BBD0
//        0x8008B884  jalr $v0                               ; the GUEST's own 0x8008BBD0(2, name)
//    0x800B390C is the title's HARDWARE-ADDRESS table (I_STAT, I_MASK, DPCR), not a BIOS vector
//    table, and 0x8008BBD0 is the guest's own interrupt-source registration into a table at
//    0x800B2888 that is ALL ZEROS in the image and filled at run time. So "on a PSX B-vector entry
//    2 is the interrupt-enable call" is a step that does not exist. The arm below still happens,
//    because it writes the device directly and the device agrees: the conclusion survived, the
//    reason did not.
//
// 2. The registered element is NOT a CD service. 0x800C1528 is `{0, handler 0x80087660, verifier
//    0x800875F8, 0}`, and 0x80087660 issues GP1(0x88) -- a VBlank display-area start. One bounded
//    run, 4,199 presented frames, 88,891,395 executed instructions: the framework handed that
//    element `I_STAT&I_MASK=0x001` 4,044 times and `=0x005` (VBlank + CD-ROM) 15 times, and it
//    correctly declined the CD bit every time. The guest then acked with `I_STAT=0x0FE` 4,059 times
//    from `ra=0x80086D90`, armed DMA channel 3 exactly once (DICR 0x00920000 -> 0x009A0000, armed
//    mask 0x1A), and produced ZERO `DMA3 complete` over the whole run, with `PSXPORT_DEBUG=cdcr`
//    logging 0 reads of the controller's data FIFO at 0x1F801802.
//
// THE ACTUAL CAUSE is not a mask bit. The guest's CD-ROM service is the BIOS hardware event class
// 0xF0000003 (HwCD). Retail CdInit at 0x8008A16C writes 0x8008A238 / 0x8008A260 / 0x8008A288 into
// the BIOS callback slots at 0x800B3B14 / 0x800B3B18 / 0x800B1C7C, and each of those three
// functions is `a0 = 0xF0000003 ; jal 0x8008F9D0` (the B0:0x07 DeliverEvent stub) with a1 = 0x20 or
// 0x40. On hardware the chain is: the controller raises IRQ2 -> the BIOS's OWN CD-ROM interrupt
// handler reads the response -> it calls the function at 0x800B3B18 -> DeliverEvent(0xF0000003,
// ...) -> the libcd handler starts DMA channel 3. `Hle::deliverEvent` has arms for SwCARD
// (0xF4000001), HwCARD (0xF0000011) and HwSPU (0xF0000009) and NONE for HwCD, and the BIOS CD-ROM
// interrupt handler is ROM code this port does not have.
//
// THIS ARM STAYS, and it is still the right device write: it is what makes IRQ2 deliverable at all,
// and the guest does ask for its data (the run's own `cd` channel shows the 71-sector STR read from
// LBA 397). But it is NECESSARY, NOT SUFFICIENT. Do not read this function as the CD-ROM service.
// The framework change that owns the rest is in `docs/issues/0026`, and it is not the title's to
// write: a spider1 override for a BIOS hardware event class would be a second implementation of the
// framework's event owner and would be a tap.
//
// The write goes through the device (0x1F801074), not to the host's mask field, so the framework
// owns the value and re-arms its own delivery gate. The CURRENT mask is read back from the same
// device and OR-ed, because the guest owns the rest of it: a title that wrote a literal here would
// clear whatever VBlank/DMA enables the guest had already asked for.
void armCdInterrupt(Core &core) {
  const uint32_t before = core.mem_r32(spider1::interruptMaskRegister);
  const uint32_t after = Spider1CdInitialization::armedMask(before);
  if (after == before) {
    lucent::info("cd",
                 "Spider-Man 1 CdInit found the CD interrupt already armed in I_MASK 0x{:03X}",
                 before);
    return;
  }
  core.mem_w32(spider1::interruptMaskRegister, after);
  lucent::info(
      "cd",
      "Spider-Man 1 CdInit armed the CD interrupt: I_MASK 0x{:03X} -> 0x{:03X}. The retail "
      "0x8008D54C reach of the B-vector enable thunk (a0=2) is the effect this override "
      "replaces, and without it Hle::irqPoll computes i_stat & i_mask = 0, so the "
      "guest's own registered CD element never runs",
      before,
      after);
}

} // namespace

void Spider1CdInitialization::installCallbackSlots(Core &core) {
  // Exact public success contract of SLUS_008.75 CdInit: install its event callbacks and return
  // true. The host owns every CD operation synchronously, so starting the guest controller reset
  // handshake would be both unobservable and a cadence violation (its IntrWait polls I_STAT bit 0).
  core.mem_w32(spider1::cdSyncCallbackSlot, spider1::cdSyncCallback);
  core.mem_w32(spider1::cdReadyCallbackSlot, spider1::cdReadyCallback);
  core.mem_w32(spider1::cdEventCallbackSlot, spider1::cdEventCallback);
  core.mem_w32(spider1::cdEventUnusedSlot, 0);
  core.r[2] = 1;
}

void Spider1CdInitialization::install(Core &core) {
  installCallbackSlots(core);
  armCdInterrupt(core);
}

} // namespace spider
