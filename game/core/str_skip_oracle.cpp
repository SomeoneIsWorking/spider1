// Runtime-only discriminator for the retail STR player's natural skip path.
#include "str_skip_oracle.h"
#include "cfg.h"
#include "core.h"
#include "game.h"
#include "override_registry.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <lucent/log.h>

extern void gen_func_8002AA0C(Core *);
extern void gen_func_8006BF9C(Core *);

namespace {
constexpr uint32_t kPlayer = 0x8002AA0C, kBoot = 0x8006BF9C;
constexpr uint32_t kPrePoll = 0x8002AEB0, kPostPoll = 0x8002AEB8, kGuard = 0x8002AEF8,
                   kTeardown = 0x8002AF90;
constexpr uint32_t kStartHeld = 0x800A4ED4, kStartEdge = 0x800A4ED5, kCrossEdge = 0x800A4E25;
enum class Drive { Start, Cross, Held };
enum class Owner : unsigned { Boot0, Boot1, Queued, Dispatcher, Other, Count };
enum class Event { PostPoll, GuardArm, Teardown };

bool parseDrive(const char *text, Drive &out) {
  if (!std::strcmp(text, "start")) {
    out = Drive::Start;
  } else if (!std::strcmp(text, "cross")) {
    out = Drive::Cross;
  } else if (!std::strcmp(text, "held")) {
    out = Drive::Held;
  } else {
    return false;
  }
  return true;
}

bool validContext(uint32_t ctx) {
  return ctx == 0x800B47F4u;
}

const char *ownerName(Owner o) {
  static const char *n[] = {"boot-id0", "boot-id1", "queued", "dispatcher", "other"};
  return n[(unsigned)o];
}
Owner ownerOf(uint32_t ra, uint8_t id) {
  if (ra == 0x8006C16C && id == 0) {
    return Owner::Boot0;
  }
  if (ra == 0x8006C188 && id == 1) {
    return Owner::Boot1;
  }
  if (ra == 0x8006BF84) {
    return Owner::Queued;
  }
  if (ra == 0x800100C8 || ra == 0x800101BC) {
    return Owner::Dispatcher;
  }
  return Owner::Other;
}
struct Call {
  uint8_t id = 0;
  Owner owner = Owner::Other;
  uint32_t ctx = 0, maxTick = 0;
  bool drove = false, pre = false, post = false, guardArm = false, teardown = false;
};

void classifyEvent(Call &call, Event event, uint32_t tick, bool selectedEdge) {
  if (tick > call.maxTick) {
    call.maxTick = tick;
  }
  if (event == Event::PostPoll) {
    call.pre |= selectedEdge && tick < 30u;
    call.post |= selectedEdge && tick >= 30u;
  } else if (event == Event::GuardArm) {
    call.guardArm = true;
  } else if (event == Event::Teardown) {
    call.teardown = true;
  }
}
struct Totals {
  unsigned calls = 0, owner[(unsigned)Owner::Count] = {}, drove = 0, pre = 0, post = 0,
           guardArm = 0, teardown = 0, clean = 0, bootReturns = 0, heldId1Suppressed = 0;
} totals;
Drive drive = Drive::Start;
Call *current = nullptr;

void observe(Core *c, uint64_t, uint32_t pc, void *) {
  if (!current) {
    return;
  }
  const uint32_t tick = c->mem_r32(current->ctx + 1708u);
  if (pc == kPrePoll && !current->drove && drive != Drive::Held && tick >= 30) {
    const uint16_t bit = drive == Drive::Start ? 0x0008u : 0x4000u;
    // Six serviced pad frames is the framework's established deterministic tap
    // duration. A two-frame pulse was live-falsified here: the movie loop
    // advanced past it before the guest pad buffer sampled it (0 post-guard
    // edges over both boot movies).
    c->game->pad.driveTap((uint16_t)(0xFFFFu & ~bit), 6);
    current->drove = true;
  } else if (pc == kPostPoll) {
    const bool edge = c->mem_r8(drive == Drive::Cross ? kCrossEdge : kStartEdge) != 0;
    classifyEvent(*current, Event::PostPoll, tick, edge);
  } else if (pc == kGuard) {
    classifyEvent(*current, Event::GuardArm, tick, false);
  } else if (pc == kTeardown) {
    classifyEvent(*current, Event::Teardown, tick, false);
  }
}

bool hasQueuedCorpus(const Totals &value) {
  return value.owner[(unsigned)Owner::Queued] != 0;
}

void report(const char *where) {
  lucent::info("strskip",
               "{} calls={} boot0={} boot1={} queued={} dispatcher={} other={} "
               "drove={} pre30={} post30={} guard_arm={} teardown={} clean={} "
               "boot_returns={} held_id1_suppressed={}",
               where,
               totals.calls,
               totals.owner[0],
               totals.owner[1],
               totals.owner[2],
               totals.owner[3],
               totals.owner[4],
               totals.drove,
               totals.pre,
               totals.post,
               totals.guardArm,
               totals.teardown,
               totals.clean,
               totals.bootReturns,
               totals.heldId1Suppressed);
  if (!hasQueuedCorpus(totals)) {
    lucent::warn("strskip", "MISSING CORPUS: queued path 0 invocation(s); no queued-path verdict");
  }
}

void player(Core *c) {
  Call call;
  call.id = (uint8_t)c->r[4];
  call.owner = ownerOf(c->r[31], call.id);
  call.ctx = c->r[28];
  if (!validContext(call.ctx)) {
    lucent::error("strskip",
                  "REFUSED call {} id={}: gp/context is 0x{:08X}, expected 0x800B47F4",
                  totals.calls + 1,
                  call.id,
                  call.ctx);
    std::abort();
  }
  ++totals.calls;
  ++totals.owner[(unsigned)call.owner];
  if (drive == Drive::Held && call.owner == Owner::Boot0) {
    c->game->pad.driveHold((uint16_t)(0xFFFFu & ~0x0008u));
  }
  constexpr uint32_t pcs[] = {kPrePoll, kPostPoll, kGuard, kTeardown};
  Call *outer = current;
  current = &call;
  // PcObserver::arm replaces an existing observer, so check first: diagnostics
  // must coexist by refusing, never silently steal another instrument's
  // selected-PC stream.
  if (c->pcObserver.armed()) {
    lucent::error("strskip",
                  "REFUSED call {} id={}: PcObserver is already owned by "
                  "another diagnostic",
                  totals.calls,
                  call.id);
    std::abort();
  }
  const bool armed = c->pcObserver.arm(pcs, 4, observe, nullptr);
  if (!armed) {
    lucent::error("strskip",
                  "REFUSED call {} id={}: PcObserver rejected four valid checkpoints",
                  totals.calls,
                  call.id);
    std::abort();
  }
  gen_func_8002AA0C(c);
  if (armed) {
    c->pcObserver.disarm();
  }
  current = outer;
  // Held must survive ID0 return so FUN_8006BF9C can exercise its own ID1
  // suppression test.
  if (drive != Drive::Held || call.owner != Owner::Boot0) {
    c->game->pad.driveRelease();
  }
  totals.drove += call.drove;
  totals.pre += call.pre;
  totals.post += call.post;
  totals.guardArm += call.guardArm;
  totals.teardown += call.teardown;
  totals.clean += c->mem_r32(call.ctx + 1680u) == 0;
  lucent::info("strskip",
               "call={} id={} owner={} max_tick={} drove={} edge_pre30={} "
               "edge_post30={} guard_arm={} common_teardown={} "
               "active_after={} held_after={}",
               totals.calls,
               call.id,
               ownerName(call.owner),
               call.maxTick,
               call.drove,
               call.pre,
               call.post,
               call.guardArm,
               call.teardown,
               c->mem_r32(call.ctx + 1680u),
               c->mem_r8(kStartHeld));
}
void boot(Core *c) {
  const unsigned id1 = totals.owner[(unsigned)Owner::Boot1];
  gen_func_8006BF9C(c);
  ++totals.bootReturns;
  if (drive == Drive::Held && totals.owner[(unsigned)Owner::Boot1] == id1) {
    ++totals.heldId1Suppressed;
  }
  c->game->pad.driveRelease();
  report("boot-owner-return");
}

} // namespace

void spiderman_install_str_skip_oracle(Game *) {
  const char *m = cfg_str("PSXPORT_STR_SKIP_ORACLE");
  if (!m || !*m) {
    return;
  }
  if (!parseDrive(m, drive)) {
    lucent::error("strskip", "REFUSED mode '{}' (start|cross|held)", m);
    std::abort();
  }
  engine_set_override_main(kPlayer, player, gen_func_8002AA0C);
  engine_set_override_main(kBoot, boot, gen_func_8006BF9C);
  lucent::info(
      "strskip", "ARMED mode={} shipping player/body/teardown retained; selected PCs=4", m);
}

int spiderman_str_skip_selftest(const char *which, const char *) {
  if (std::strcmp(which, "strskip")) {
    return 2;
  }
  Call early;
  classifyEvent(early, Event::PostPoll, 29, true);
  classifyEvent(early, Event::GuardArm, 29, false);
  Call late;
  classifyEvent(late, Event::PostPoll, 29, false);
  classifyEvent(late, Event::PostPoll, 30, true);
  classifyEvent(late, Event::Teardown, 30, false);
  Call held;
  classifyEvent(held, Event::PostPoll, 0, true);
  classifyEvent(held, Event::PostPoll, 30, false);
  Drive parsed = Drive::Held;
  Totals absent;
  absent.owner[(unsigned)Owner::Boot0] = 1;
  const bool modes = parseDrive("start", parsed) && parsed == Drive::Start &&
                     parseDrive("cross", parsed) && parsed == Drive::Cross &&
                     parseDrive("held", parsed) && parsed == Drive::Held &&
                     !parseDrive("bogus", parsed);
  const bool owners =
      ownerOf(0x8006C16C, 0) == Owner::Boot0 && ownerOf(0x8006C188, 1) == Owner::Boot1 &&
      ownerOf(0x8006BF84, 7) == Owner::Queued && ownerOf(0x800101BC, 7) == Owner::Dispatcher;
  const bool context = validContext(0x800B47F4u) && !validContext(0) && !validContext(0x800B47F0u);
  const bool corpus = !hasQueuedCorpus(absent);
  const bool ok = early.pre && early.guardArm && !early.post && !early.teardown && late.post &&
                  late.teardown && held.pre && !held.post && !held.teardown && modes && owners &&
                  context && corpus;
  lucent::info("strskip",
               "selftest early_suppressed={} late_exits={} "
               "held_not_retriggered={} result={}",
               early.pre && !early.teardown,
               late.post && late.teardown,
               held.pre && !held.post && !held.teardown,
               ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}
