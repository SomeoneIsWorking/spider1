#include "spider1_widescreen.h"

#include "core.h"
#include "game.h"
#include "gpu_vk.h"
#include "mods.h"
#include "native_execution.h"

#include <cstdlib>
#include <limits>
#include <lucent/log.h>

namespace spider::spider1 {
namespace {

// $a0 carries the environment record into the constructor 0x800884C0.
constexpr int kEnvironmentArgument = 4;

// $a1 carries the viewport record into the projection publication 0x80075D0C.
constexpr int kRecordArgument = 5;

// The guest stores with `sh`, so its arithmetic wraps at the u16 boundary.
constexpr std::uint32_t kFieldModulus = 0x10000u;

[[noreturn]] void refuse(const char *what) {
  lucent::error("spider1-wide", "SLUS_008.75 guest widescreen {}", what);
  std::abort();
}

std::uint16_t wrapField(std::uint32_t value) {
  return static_cast<std::uint16_t>(value % kFieldModulus);
}

// Signed distance between two u16 window edges in the guest's wrapping arithmetic.
std::uint16_t fieldSpan(std::uint16_t far, std::uint16_t near) {
  return wrapField(static_cast<std::uint32_t>(far) + kFieldModulus -
                   static_cast<std::uint32_t>(near));
}

// A guest pointer in main RAM, KSEG0 or KSEG1 form; anything else is a wiring defect.
constexpr std::uint32_t kKseg0Base = 0x80000000u;
constexpr std::uint32_t kKseg1Base = 0xA0000000u;
constexpr std::uint32_t kMainRamBytes = 0x00200000u;
constexpr std::uint32_t kParallelRamBytes = 0x00100000u;

Spider1Widescreen &ownerFrom(Core *core, const char *site) {
  if (!core || !core->runtime) {
    lucent::error("spider1-wide", "SLUS_008.75 {} override ran without its title runtime", site);
    std::abort();
  }
  // A null or foreign policy is a wiring defect and stops the run.
  auto *const policy = dynamic_cast<Spider1Widescreen *>(
      const_cast<GuestWidescreenProjection *>(core->runtime->guestWidescreenProjection()));
  if (!policy) {
    lucent::error("spider1-wide", "SLUS_008.75 {} override reached another title's policy", site);
    std::abort();
  }
  return *policy;
}

// The retail body is the original guest function executed through Lightrec.
void originalProjection(Core &core) {
  psx::cpu::callOriginalToReturn(core,
                                 kProjectionPublication,
                                 psx::cpu::ExecutionBudget::currentTurn(core),
                                 "spider1-wide::projection original");
}

void originalDrawEnvironment(Core &core) {
  psx::cpu::callOriginalToReturn(core,
                                 kDrawEnvironmentConstructor,
                                 psx::cpu::ExecutionBudget::currentTurn(core),
                                 "spider1-wide::draw environment original");
}

void projectionOverride(Core *core) {
  ownerFrom(core, "projection").publishProjection(*core, originalProjection);
}

void drawEnvironmentOverride(Core *core) {
  ownerFrom(core, "draw environment").publishDrawEnvironment(*core, originalDrawEnvironment);
}

} // namespace

Spider1Widescreen::Spider1Widescreen(Latch latch) : latch_(latch) {
  if (!latch_) {
    refuse("requires the shared plan latch");
  }
}

Spider1Widescreen &Spider1Widescreen::from(Core &core) {
  return ownerFrom(&core, "frame boundary");
}

bool Spider1Widescreen::isGuestRam(std::uint32_t address) {
  // Null and physical address 0 are never a title-owned record.
  if (address == 0) {
    return false;
  }
  if (address >= kKseg0Base && address - kKseg0Base < kMainRamBytes) {
    return true;
  }
  if (address >= kKseg1Base && address - kKseg1Base < kParallelRamBytes) {
    return true;
  }
  return address < kMainRamBytes;
}

GuestProjectionGeometry Spider1Widescreen::measuredGeometry(std::uint16_t horizontalSpan,
                                                            std::uint16_t verticalSpan,
                                                            std::uint32_t drawWidth) {
  if (horizontalSpan == 0 || verticalSpan == 0 || drawWidth == 0 ||
      drawWidth > std::numeric_limits<std::uint16_t>::max()) {
    lucent::error("spider1-wide",
                  "SLUS_008.75 published an unusable projection extent {}x{} with draw width {}",
                  horizontalSpan,
                  verticalSpan,
                  drawWidth);
    std::abort();
  }
  return {{static_cast<int>(horizontalSpan), static_cast<int>(verticalSpan)},
          static_cast<int>(drawWidth)};
}

GuestProjectionPlan Spider1Widescreen::relatch(Core &core, GuestProjectionGeometry geometry) {
  GuestProjectionPlan latched = latch_(&core, geometry);
  if (latched.projectionCenterX <= 0 || latched.guestDrawWidth <= 0 ||
      latched.projectionCenterX > std::numeric_limits<std::uint16_t>::max() ||
      latched.guestDrawWidth > std::numeric_limits<std::uint16_t>::max()) {
    lucent::error("spider1-wide",
                  "the framework returned an unusable guest projection (centre={}, draw width={})",
                  latched.projectionCenterX,
                  latched.guestDrawWidth);
    std::abort();
  }
  plan_ = latched;
  return latched;
}

void Spider1Widescreen::publishProjection(Core &core, const RetailBody &retail) {
  if (!core.game) {
    refuse("reached a Core with no Game");
  }
  if (!retail) {
    refuse("projection publication requires the retail guest body");
  }
  if (!drawWidthMeasured()) {
    // RECT.w comes from graphical init (0x80061140), which runs before the render walk reaches
    // 0x80075D0C.
    refuse("projection publication ran before the guest published a draw environment");
  }

  // The record is the call's $a1; the cell only holds it after the body's first run.
  const std::uint32_t record = core.r[kRecordArgument];
  if (!isGuestRam(record)) {
    lucent::error("spider1-wide",
                  "SLUS_008.75 projection publication received viewport record 0x{:08X}, which is "
                  "not guest memory",
                  record);
    std::abort();
  }

  const std::uint16_t far = core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar);
  const std::uint16_t near = core.mem_r16(record + Spider1ViewportOffset::kHorizontalNear);
  const std::uint16_t centreX = core.mem_r16(record + Spider1ViewportOffset::kCentreX);

  // A uniform shift leaves far - near alone, so H stays; the lens divisor is neither read nor
  // written.
  const std::uint16_t span = fieldSpan(far, near);
  const std::uint16_t verticalSpan =
      fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kVerticalFar),
                core.mem_r16(record + Spider1ViewportOffset::kVerticalNear));
  const GuestProjectionPlan latched =
      relatch(core, measuredGeometry(span, verticalSpan, measuredDrawWidth_));
  appliedMargin_ = latched.projectionHorizontalMargin;
  if (!latched.widescreen()) {
    // 4:3 never writes, so it is retail byte for byte; unwidening restores the captured retail
    // tuple.
    if (retailCaptured_) {
      core.mem_w16(record + Spider1ViewportOffset::kHorizontalFar, retail_.horizontalFar);
      core.mem_w16(record + Spider1ViewportOffset::kHorizontalNear, retail_.horizontalNear);
      core.mem_w16(record + Spider1ViewportOffset::kCentreX, retail_.centreX);
    }
    retail(core);
    return;
  }

  // Applied only where the record does not already hold the margin, so it never compounds.
  const std::uint32_t margin = static_cast<std::uint32_t>(latched.projectionHorizontalMargin);
  const auto shifted = [margin](std::uint16_t value) {
    return wrapField(static_cast<std::uint32_t>(value) + margin);
  };
  if (!retailCaptured_ || far != shifted(retail_.horizontalFar) ||
      near != shifted(retail_.horizontalNear)) {
    // Nothing published yet, or the guest re-authored the window: the record holds retail values
    // now.
    captureRetail(core, record);
    lucent::info("spider1-wide",
                 "guest viewport window {}..{} (centre {}) -> {}..{} (margin {}), draw width {}",
                 far,
                 near,
                 centreX,
                 shifted(far),
                 shifted(near),
                 margin,
                 measuredDrawWidth_);
  }

  // Focal-length inputs (span, lens divisor) sampled before the body runs; they must survive
  // untouched.
  const std::uint16_t retailCentreX = centreX;
  const std::uint16_t retailLens = core.mem_r16(record + Spider1ViewportOffset::kLensDivisor);
  const std::uint16_t distanceBefore =
      core.mem_r16(record + Spider1ViewportOffset::kScreenDistance);

  // Every write is retail + margin, never the value just read, so republishing is idempotent.
  // Only the horizontal pair moves; the body re-asserts the depth window.
  core.mem_w16(record + Spider1ViewportOffset::kHorizontalFar, shifted(retail_.horizontalFar));
  core.mem_w16(record + Spider1ViewportOffset::kHorizontalNear, shifted(retail_.horizontalNear));

  retail(core);

  // A uniform shift must not change the focal-length inputs.
  const std::uint16_t publishedSpan =
      fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar),
                core.mem_r16(record + Spider1ViewportOffset::kHorizontalNear));
  if (publishedSpan != span ||
      core.mem_r16(record + Spider1ViewportOffset::kLensDivisor) != retailLens) {
    lucent::error("spider1-wide",
                  "the widened window spans {} with lens divisor {}; retail spans {} with {}. A "
                  "horizontal widening must not move the focal length's own inputs",
                  publishedSpan,
                  core.mem_r16(record + Spider1ViewportOffset::kLensDivisor),
                  span,
                  retailLens);
    std::abort();
  }

  // The title derives OFX, OFY and H, so the shifted window stays. FUN_8007C2AC and FUN_8007B9CC
  // re-assert CR24/CR25 from this record per vertex.

  // The centre is not written here: the title's derivation produces it and the plan's centre must
  // agree.
  const std::uint16_t publishedCentreX = core.mem_r16(record + Spider1ViewportOffset::kCentreX);
  if (publishedCentreX != latched.projectionCenterX) {
    lucent::error(
        "spider1-wide",
        "the title derived OFX {} but the plan widens to centre {}: the plan and the "
        "guest's own derivation disagree, so the frame would not be the widening it claims",
        publishedCentreX,
        latched.projectionCenterX);
    std::abort();
  }
  // H is unchanged when span and lens are.
  published_ = true;
  lucent::info("spider1-wide",
               "published guest OFX {} (retail {}), OFY {}, H {} (was {}), span {}, lens {} — the "
               "frustum grew by OFX at an unchanged focal length",
               publishedCentreX,
               retailCentreX,
               core.mem_r16(record + Spider1ViewportOffset::kCentreY),
               core.mem_r16(record + Spider1ViewportOffset::kScreenDistance),
               distanceBefore,
               publishedSpan,
               retailLens);
}

void Spider1Widescreen::publishDrawEnvironment(Core &core, const RetailBody &retail) {
  if (!core.game) {
    refuse("reached a Core with no Game");
  }
  if (!retail) {
    refuse("draw-environment publication requires the retail guest body");
  }
  const std::uint32_t environment = core.r[kEnvironmentArgument];
  if (!isGuestRam(environment)) {
    lucent::error(
        "spider1-wide",
        "the draw-environment publication received 0x{:08X}, which is not guest memory. "
        "The guest's environments live in its own resident data (the boot publishes them at "
        "0x8009A6E4 / 0x8009A740 and 0x8009A75C / 0x8009A7B8), so a refusal here is a wrong "
        "bound or a wrong argument register, not a title that has no draw environment",
        environment);
    std::abort();
  }

  retail(core);

  // The body has written the whole RECT; 0x800884C0 takes RECT.h as a fifth argument at a
  // frame-dependent offset.
  const std::uint32_t retailWidth = core.mem_r16(environment + 4);
  const std::uint16_t retailHeight = core.mem_r16(environment + 6);
  measuredDrawWidth_ = retailWidth;
  const GuestProjectionPlan latched =
      relatch(core, measuredGeometry(wrapField(retailWidth), retailHeight, retailWidth));
  if (latched.widescreen()) {
    core.mem_w16(environment + 4, static_cast<std::uint16_t>(latched.guestDrawWidth));
  }
  lucent::info("spider1-wide",
               "guest draw clip {}x{} -> {}x{} (RECT.w only; x, y, h and every other field are the "
               "retail body's)",
               retailWidth,
               retailHeight,
               latched.guestDrawWidth,
               latched.guestClipRight + 1);
}

void Spider1Widescreen::synchronizePresentation(Core &core) {
  // Needs only the draw environment published; the publication need not have run, or the canvas
  // would be stale between graphical init and the first publication.
  if (!drawWidthMeasured()) {
    return;
  }
  const std::uint32_t record = core.mem_r32(kViewportRecordCell);
  if (!isGuestRam(record)) {
    // 0x80075DB0 installs this cell; until then the plan from the draw-environment site stands.
    return;
  }
  // Span is the same whether the window is retail or widened.
  const GuestProjectionPlan latched = relatch(
      core,
      measuredGeometry(fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar),
                                 core.mem_r16(record + Spider1ViewportOffset::kHorizontalNear)),
                       fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kVerticalFar),
                                 core.mem_r16(record + Spider1ViewportOffset::kVerticalNear)),
                       measuredDrawWidth_));
  if (appliedMargin_ == latched.projectionHorizontalMargin) {
    // The record already matches the plan; the host extent was re-latched above.
    return;
  }
  appliedMargin_ = latched.projectionHorizontalMargin;
  if (!retailCaptured_) {
    // No retail baseline yet; the next publication widens the record.
    return;
  }
  if (latched.widescreen()) {
    // Widens with no guest call; these are the values the publication's derivation produces.
    core.mem_w16(record + Spider1ViewportOffset::kHorizontalFar,
                 wrapField(static_cast<std::uint32_t>(retail_.horizontalFar) +
                           static_cast<std::uint32_t>(latched.projectionHorizontalMargin)));
    core.mem_w16(record + Spider1ViewportOffset::kHorizontalNear,
                 wrapField(static_cast<std::uint32_t>(retail_.horizontalNear) +
                           static_cast<std::uint32_t>(latched.projectionHorizontalMargin)));
    core.mem_w16(record + Spider1ViewportOffset::kCentreX,
                 static_cast<std::uint16_t>(latched.projectionCenterX));
    published_ = true;
    lucent::info("spider1-wide",
                 "aspect changed at the frame boundary: guest OFX {} -> {}, window margin {}, host "
                 "canvas {} (native {})",
                 retail_.centreX,
                 latched.projectionCenterX,
                 latched.projectionHorizontalMargin,
                 latched.presentationExtent.width,
                 latched.nativeExtent.width);
    return;
  }
  // Unwidening. Put the captured retail tuple back, or a 4:3 canvas would keep a wide frustum.
  core.mem_w16(record + Spider1ViewportOffset::kHorizontalFar, retail_.horizontalFar);
  core.mem_w16(record + Spider1ViewportOffset::kHorizontalNear, retail_.horizontalNear);
  core.mem_w16(record + Spider1ViewportOffset::kCentreX, retail_.centreX);
  lucent::info("spider1-wide",
               "aspect returned to 4:3 at the frame boundary: guest OFX {} restored from {}, host "
               "canvas {}",
               retail_.centreX,
               latched.projectionCenterX,
               latched.presentationExtent.width);
}

void Spider1Widescreen::captureRetail(Core &core, std::uint32_t record) {
  retail_.horizontalFar = core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar);
  retail_.horizontalNear = core.mem_r16(record + Spider1ViewportOffset::kHorizontalNear);
  retail_.centreX = core.mem_r16(record + Spider1ViewportOffset::kCentreX);
  retailCaptured_ = true;
}

void installSpider1Widescreen(Core &core) {
  installNativeOverride(
      core, kProjectionPublication, "Spider projection publication", projectionOverride);
  installNativeOverride(
      core, kDrawEnvironmentConstructor, "Spider draw environment", drawEnvironmentOverride);
  lucent::info("spider1-wide",
               "guest widescreen installed: projection publication 0x{:08X}, draw environment "
               "0x{:08X}",
               kProjectionPublication,
               kDrawEnvironmentConstructor);
}

} // namespace spider::spider1
