#include "allocator_audit.h"

#include "cfg.h"
#include "core.h"
#include "game.h"
#include "spider_context.h"

#include <cstdlib>
#include <execinfo.h>
#include <lucent/log.h>

namespace spider {
namespace {

constexpr uint32_t kFreeListOffset = 0xDC4u;
constexpr uint32_t kRamMirrorLo = 0x80000000u;
constexpr uint32_t kRamMirrorHi = 0x80800000u;
constexpr unsigned kArenaCount = 2;
constexpr unsigned kMaxNodes = 1u << 18;

bool validNode(uint32_t address) {
  return address >= kRamMirrorLo && address + 8u <= kRamMirrorHi && (address & 3u) == 0;
}

[[noreturn]] void fail(Core &core,
                       const char *boundary,
                       unsigned arena,
                       uint32_t predecessor,
                       uint32_t invalid,
                       const char *reason) {
  lucent::error("allocaudit",
                "FIRST INVALID FREE-LIST EDGE at {}: arena={} predecessor={:08X} next={:08X} "
                "reason={} frame-pc={:08X} ra={:08X} sp={:08X} irq-active={}",
                boundary,
                arena,
                predecessor,
                invalid,
                reason,
                core.pc,
                core.r[31],
                core.r[29],
                core.game ? core.game->hle.in_irq : 0);
  void *frames[32];
  const int count = backtrace(frames, 32);
  backtrace_symbols_fd(frames, count, 2);
  std::abort();
}

} // namespace

void AllocatorAudit::verifyFreeLists(Core &core, const char *boundary) {
  for (unsigned arena = 0; arena < kArenaCount; ++arena) {
    const uint32_t headAddress = core.r[28] + kFreeListOffset + arena * 4u;
    uint32_t predecessor = headAddress;
    uint32_t node = core.mem_r32(headAddress);
    unsigned visited = 0;
    while (node != 0) {
      if (!validNode(node)) {
        fail(core, boundary, arena, predecessor, node, "unmapped-or-unaligned node");
      }
      if (predecessor != headAddress && node <= predecessor) {
        fail(core, boundary, arena, predecessor, node, "list is not strictly address-sorted");
      }
      if (++visited > kMaxNodes) {
        fail(core, boundary, arena, predecessor, node, "cycle or impossible node count");
      }
      predecessor = node;
      node = core.mem_r32(node);
    }
  }
}

void AllocatorAudit::captureFreeHeaders(Core &core) {
  freeHeaders_.clear();
  for (unsigned arena = 0; arena < kArenaCount; ++arena) {
    uint32_t node = core.mem_r32(core.r[28] + kFreeListOffset + arena * 4u);
    unsigned visited = 0;
    while (node != 0 && validNode(node) && visited++ < kMaxNodes) {
      freeHeaders_.insert(node);
      node = core.mem_r32(node);
    }
  }
}

void AllocatorAudit::storeCallback(Core *core, uint32_t address, uint32_t value, uint32_t width) {
  context(*core).allocatorAudit.observeStore(*core, address, value, width);
}

void AllocatorAudit::observeStore(Core &core, uint32_t address, uint32_t value, uint32_t width) {
  if (!enabled_ || freeHeaders_.empty()) {
    return;
  }
  uint32_t header = 0;
  for (uint32_t byte = 0; byte < width; ++byte) {
    const uint32_t candidate = (address + byte) & ~3u;
    if (freeHeaders_.find(candidate) != freeHeaders_.end()) {
      header = candidate;
      break;
    }
  }
  if (!header) {
    return;
  }
  // Generated straight-line bodies do not continuously update Core::pc, so it can still name the
  // HLE callsite while the host stack is executing the allocator. The scoped depth is the reliable
  // owner marker. Keep IRQ writes observable: an interrupt may run inside rec_irq_poll while the
  // allocator is live, and is not allocator-owned merely because it interrupted that scope.
  if (depth_ != 0 && (!core.game || core.game->hle.in_irq == 0)) {
    return;
  }
  lucent::error("allocaudit",
                "FIRST EXTERNAL WRITE TO A LIVE FREE-NODE LINK: header={:08X} old-next={:08X} "
                "store-address={:08X} value={:08X} width={} pc={:08X} ra={:08X} sp={:08X} "
                "allocator-depth={} irq-active={}",
                header,
                core.mem_r32(header),
                address,
                value,
                width,
                core.pc,
                core.r[31],
                core.r[29],
                depth_,
                core.game ? core.game->hle.in_irq : 0);
  void *frames[32];
  const int count = backtrace(frames, 32);
  backtrace_symbols_fd(frames, count, 2);
  std::abort();
}

void AllocatorAudit::arm(Core &core) {
  if (!initialized_) {
    initialized_ = true;
    enabled_ = cfg_dbg("allocaudit");
    if (enabled_) {
      cfg_logi("allocaudit",
               "ARMED on the existing allocator/free owner; validates both executable-derived "
               "address-sorted free lists at every boundary and refuses allocator re-entry");
      if (core.storeWatchCb) {
        lucent::error(
            "allocaudit",
            "cannot arm: another programmatic store-watch callback already owns this Core");
        std::abort();
      }
      core.storeWatchCb = storeCallback;
      core.wwatch_arm(kRamMirrorLo, kRamMirrorHi);
    }
  }
}

void AllocatorAudit::enter(Core &core, const char *operation, uint32_t argument) {
  arm(core);
  if (!enabled_) {
    return;
  }
  ++calls_;
  if (depth_ != 0) {
    lucent::error("allocaudit",
                  "FIRST ALLOCATOR RE-ENTRY: operation={} argument={:08X} outer-depth={} call={} "
                  "pc={:08X} ra={:08X} sp={:08X} irq-active={}",
                  operation,
                  argument,
                  depth_,
                  calls_,
                  core.pc,
                  core.r[31],
                  core.r[29],
                  core.game ? core.game->hle.in_irq : 0);
    void *frames[32];
    const int count = backtrace(frames, 32);
    backtrace_symbols_fd(frames, count, 2);
    std::abort();
  }
  verifyFreeLists(core, operation);
  captureFreeHeaders(core);
  ++depth_;
}

void AllocatorAudit::heapInitEnter(Core &core) {
  arm(core);
  if (!enabled_) {
    return;
  }
  if (depth_ != 0) {
    lucent::error("allocaudit",
                  "FIRST ALLOCATOR RE-ENTRY: operation=heap-init outer-depth={} pc={:08X} "
                  "ra={:08X} sp={:08X} irq-active={}",
                  depth_,
                  core.pc,
                  core.r[31],
                  core.r[29],
                  core.game ? core.game->hle.in_irq : 0);
    std::abort();
  }
  ++calls_;
  ++depth_;
}

void AllocatorAudit::heapInitLeave(Core &core) {
  leave(core, "heap-init-exit");
}

void AllocatorAudit::leave(Core &core, const char *operation) {
  if (!enabled_) {
    return;
  }
  if (depth_ != 1) {
    lucent::error("allocaudit", "internal depth mismatch leaving {}: {}", operation, depth_);
    std::abort();
  }
  --depth_;
  verifyFreeLists(core, operation);
  captureFreeHeaders(core);
}

void AllocatorAudit::allocatorEnter(Core &core, uint32_t size, uint32_t arena, uint32_t flag) {
  if (initialized_ && enabled_ && calls_ < 16) {
    cfg_logf("allocaudit", "alloc request size=%u arena=%08X flag=%u", size, arena, flag);
  }
  enter(core, "allocator-entry", size);
}

void AllocatorAudit::allocatorLeave(Core &core, uint32_t result) {
  if (enabled_ && calls_ <= 16) {
    cfg_logf("allocaudit", "alloc result=%08X", result);
  }
  leave(core, "allocator-exit");
}

void AllocatorAudit::freeEnter(Core &core, uint32_t block) {
  enter(core, "free-entry", block);
}

void AllocatorAudit::freeLeave(Core &core) {
  leave(core, "free-exit");
}

void AllocatorAudit::resizeEnter(Core &core, uint32_t block, uint32_t size) {
  (void)size;
  enter(core, "resize-entry", block);
}

void AllocatorAudit::resizeLeave(Core &core) {
  leave(core, "resize-exit");
}

} // namespace spider
