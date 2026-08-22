#include "cfg.h"
#include "core.h"

#include <cstdlib>
#include <lucent/log.h>

#if defined(SPIDER_IRQ_POLL_AUDIT_ENABLED)

extern "C" void __real_rec_irq_poll(Core *core);

namespace {

const char *registerName(unsigned index) {
  static constexpr const char *kNames[32] = {
      "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3", "t0", "t1", "t2",
      "t3",   "t4", "t5", "t6", "t7", "s0", "s1", "s2", "s3", "s4", "s5",
      "s6",   "s7", "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra",
  };
  return kNames[index];
}

} // namespace

// Link-time diagnostic wrapper around the framework's real deferred-work gate. The gate promises
// to be invisible to the interrupted guest instruction stream, so every GPR, hi/lo and the
// diagnostic pc must survive. PSXPORT_DEBUG=allocaudit turns the comparison on; otherwise this is a
// single tail call to the shipping implementation.
extern "C" void __wrap_rec_irq_poll(Core *core) {
  if (!cfg_dbg("allocaudit")) {
    __real_rec_irq_poll(core);
    return;
  }

  const R3000 before = *static_cast<R3000 *>(core);
  const int pending = core->pending_work;
  __real_rec_irq_poll(core);
  const R3000 after = *static_cast<R3000 *>(core);

  bool changed = false;
  for (unsigned index = 0; index < 32; ++index) {
    if (before.r[index] == after.r[index]) {
      continue;
    }
    lucent::error("allocaudit",
                  "DEFERRED-WORK POLL CLOBBERED {}: {:08X} -> {:08X} entry-pc={:08X} "
                  "entry-ra={:08X} pending={}",
                  registerName(index),
                  before.r[index],
                  after.r[index],
                  before.pc,
                  before.r[31],
                  pending);
    changed = true;
  }
  if (before.hi != after.hi || before.lo != after.lo || before.pc != after.pc) {
    lucent::error("allocaudit",
                  "DEFERRED-WORK POLL CLOBBERED special state: hi {:08X}->{:08X} lo "
                  "{:08X}->{:08X} pc {:08X}->{:08X} entry-ra={:08X} pending={}",
                  before.hi,
                  after.hi,
                  before.lo,
                  after.lo,
                  before.pc,
                  after.pc,
                  before.r[31],
                  pending);
    changed = true;
  }
  if (changed) {
    std::abort();
  }
}

#endif
