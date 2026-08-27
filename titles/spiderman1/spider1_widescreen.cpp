#include "spider1_widescreen.h"

#include "core.h"
#include "game_runtime.h"
#include "gpu_vk.h"
#include "override_registry.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <lucent/log.h>

extern void gen_func_80075D0C(Core *);

namespace spider {
namespace {

constexpr uint32_t kWorldRender = 0x80075D0Cu;
constexpr uint32_t kViewportLeft = 0u;
constexpr uint32_t kViewportTop = 2u;
constexpr uint32_t kViewportRight = 4u;
constexpr uint32_t kViewportBottom = 6u;
constexpr uint32_t kViewportLensDivisor = 12u;

uint16_t checkedU16(uint32_t value, const char *field) {
  if (value > std::numeric_limits<uint16_t>::max()) {
    lucent::error("wide", "Spider-Man 1 {} {} does not fit the retail u16 viewport", field, value);
    std::abort();
  }
  return static_cast<uint16_t>(value);
}

int distance(uint16_t first, uint16_t second) {
  return std::abs(static_cast<int>(first) - static_cast<int>(second));
}

} // namespace

Spider1ViewportHorizontal spider1ProjectViewport(Spider1ViewportHorizontal native,
                                                 int projectedWidth) {
  const int nativeWidth = distance(native.left, native.right);
  if (nativeWidth <= 0 || native.lensDivisor == 0 || projectedWidth <= 0) {
    lucent::error("wide",
                  "Spider-Man 1 cannot project viewport left={} right={} lens={} to width {}",
                  native.left,
                  native.right,
                  native.lensDivisor,
                  projectedWidth);
    std::abort();
  }

  Spider1ViewportHorizontal projected = native;
  if (native.left >= native.right) {
    projected.left = checkedU16(static_cast<uint32_t>(native.right) + projectedWidth, "left");
  } else {
    projected.right = checkedU16(static_cast<uint32_t>(native.left) + projectedWidth, "right");
  }
  const uint32_t scaledLens =
      (static_cast<uint32_t>(native.lensDivisor) * projectedWidth + nativeWidth / 2) /
      static_cast<uint32_t>(nativeWidth);
  projected.lensDivisor = checkedU16(std::max(1u, scaledLens), "lens divisor");
  return projected;
}

PresentationAspect Spider1Widescreen::presentationAspect(const Core &) const {
  return PresentationAspect::Wide16x9;
}

void Spider1Widescreen::install() const {
  engine_set_override_main(kWorldRender, publishAndRenderOverride, gen_func_80075D0C);
  lucent::info("wide",
               "Spider-Man 1 guest widescreen owns world projection/culling at 0x{:08X}",
               kWorldRender);
}

void Spider1Widescreen::publishAndRender(Core &core) const {
  const uint32_t viewport = core.r[5];
  const Spider1ViewportHorizontal observed{
      .left = core.mem_r16(viewport + kViewportLeft),
      .right = core.mem_r16(viewport + kViewportRight),
      .lensDivisor = core.mem_r16(viewport + kViewportLensDivisor),
  };
  const int nativeWidth = distance(observed.left, observed.right);
  const int nativeHeight =
      distance(core.mem_r16(viewport + kViewportTop), core.mem_r16(viewport + kViewportBottom));
  const GuestProjectionPlan plan = gpu_vk_latch_guest_projection(
      &core, {.extent = {.width = nativeWidth, .height = nativeHeight}, .drawWidth = nativeWidth});
  const Spider1ViewportHorizontal projected =
      spider1ProjectViewport(observed, plan.projectionExtent.width);
  if (!initialized_ || !(observed == native_) || !(projected == applied_)) {
    initialized_ = true;
    native_ = observed;
    applied_ = projected;
    lucent::info("wide",
                 "Spider-Man 1 viewport {}x{} -> {}x{} (lens {} -> {})",
                 nativeWidth,
                 nativeHeight,
                 plan.projectionExtent.width,
                 plan.projectionExtent.height,
                 observed.lensDivisor,
                 projected.lensDivisor);
  }
  core.mem_w16(viewport + kViewportLeft, projected.left);
  core.mem_w16(viewport + kViewportRight, projected.right);
  core.mem_w16(viewport + kViewportLensDivisor, projected.lensDivisor);

  gen_func_80075D0C(&core);

  // These three fields are inputs to the retail renderer, not persistent host configuration.
  // The body rewrites the lens divisor but leaves the widened bounds in place; retaining that
  // mixed descriptor made each later call treat the previous 16:9 width as a new 4:3 baseline
  // (512 -> 684 -> 912 -> 1024 in the first live reach). Restore all inputs as one unit after the
  // renderer has consumed them so the next call always widens the game's current native viewport.
  core.mem_w16(viewport + kViewportLeft, observed.left);
  core.mem_w16(viewport + kViewportRight, observed.right);
  core.mem_w16(viewport + kViewportLensDivisor, observed.lensDivisor);
}

void Spider1Widescreen::publishAndRenderOverride(Core *core) {
  if (!core || !core->runtime) {
    lucent::error("wide", "Spider-Man 1 projection override ran without its title runtime");
    std::abort();
  }
  const auto *projection =
      dynamic_cast<const Spider1Widescreen *>(core->runtime->guestWidescreenProjection());
  if (!projection) {
    lucent::error("wide", "Spider-Man 1 projection override reached another title's policy");
    std::abort();
  }
  projection->publishAndRender(*core);
}

} // namespace spider
