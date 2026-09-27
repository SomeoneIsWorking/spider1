#include "spider1_widescreen.h"

#include "core.h"
#include "game.h"
#include "gpu_vk.h"
#include "mods.h"
#include "native_execution.h"

#include <cstdlib>
#include <limits>
#include <lucent/log.h>

namespace spider {
namespace {

// $a0 carries the environment record into the draw-environment constructor (0x800884C0 stores it to
// $v0 and writes every field through it).
constexpr int kEnvironmentArgument = 4;

// The widest value a u16 field can hold. The guest's own stores are `sh`, so its arithmetic wraps
// at this boundary and so must the owner's, or the widened record would not be the record the guest
// would have produced.
constexpr std::uint32_t kFieldModulus = 0x10000u;

[[noreturn]] void refuse(const char *what) {
  lucent::error("spider1-wide", "SLUS_008.75 guest widescreen {}", what);
  std::abort();
}

std::uint16_t wrapField(std::uint32_t value) {
  return static_cast<std::uint16_t>(value % kFieldModulus);
}

// The signed distance between two u16 window edges, in the guest's own wrapping arithmetic: the
// record states its window as two 16-bit values, so a window that straddles zero is a window whose
// edges differ by more than they appear to.
std::uint16_t fieldSpan(std::uint16_t far, std::uint16_t near) {
  return wrapField(static_cast<std::uint32_t>(far) + kFieldModulus -
                   static_cast<std::uint32_t>(near));
}

// A guest pointer the title may read and write: the 2 MiB of main RAM, in either KSEG0 (uncached)
// or KSEG1 (cached) form. Anything else — a null, a scratchpad address, a stale value from before
// the engine allocated the record — is a wiring defect, and naming it is better than a wild access.
// MEASURED 2026-09-27: the first live run refused the draw environment at 0x8009A6E4 as "not guest
// memory", and 0x8009A6E4 is 0x9A6E4 bytes into the 2 MiB image — plainly inside main RAM, and
// inside the title's own resident data. The original bound said `address < 0x80200000` after
// subtracting 0x80000000, which is the same 2 MiB; the real mistake was treating the KSEG0 ADDRESS
// as if it were already a physical offset. The subtraction is done explicitly here so the two forms
// cannot be confused again, and the bound is stated once.
constexpr std::uint32_t kKseg0Base = 0x80000000u;
constexpr std::uint32_t kKseg1Base = 0xA0000000u;
constexpr std::uint32_t kMainRamBytes = 0x00200000u;
constexpr std::uint32_t kParallelRamBytes = 0x00100000u;

Spider1Widescreen &ownerFrom(Core *core, const char *site) {
  if (!core || !core->runtime) {
    lucent::error("spider1-wide", "SLUS_008.75 {} override ran without its title runtime", site);
    std::abort();
  }
  // The policy is reached as a const base pointer because that is the framework's seam, so the
  // per-Core state behind it comes back through a checked downcast. A null or foreign result is a
  // wiring defect and stops the run rather than quietly presenting a 4:3 picture under a wide
  // claim.
  auto *const policy = dynamic_cast<Spider1Widescreen *>(
      const_cast<GuestWidescreenProjection *>(core->runtime->guestWidescreenProjection()));
  if (!policy) {
    lucent::error("spider1-wide", "SLUS_008.75 {} override reached another title's policy", site);
    std::abort();
  }
  return *policy;
}

// The retail body is always the AUTHENTICATED original guest function executed through Lightrec, so
// the transformation this owner applies cannot drift from what the guest really publishes.
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

PresentationAspect Spider1Widescreen::presentationAspect(const Core &core) const {
  if (!core.game) {
    return PresentationAspect::Standard4x3;
  }
  // Mods is the one source of truth the player edits live, and `Mods::init` has already refused the
  // enhancements this widescreen-only title does not ship. ASPECT_AUTO is NOT folded to 16:9 here:
  // it resolves against the live sink inside the plan builder, so a headless run with no wide sink
  // correctly resolves to 4:3 instead of claiming a widening it did not perform.
  switch (core.game->mods.aspect) {
  case ASPECT_4_3:
    return PresentationAspect::Standard4x3;
  case ASPECT_16_9:
    return PresentationAspect::Wide16x9;
  case ASPECT_21_9:
    return PresentationAspect::UltraWide21x9;
  case ASPECT_AUTO:
    return PresentationAspect::MatchSink;
  default:
    lucent::error("spider1-wide", "invalid aspect selector {}", core.game->mods.aspect);
    std::abort();
  }
}

Spider1Widescreen &Spider1Widescreen::from(Core &core) {
  return ownerFrom(&core, "frame boundary");
}

bool Spider1Widescreen::isGuestRam(std::uint32_t address) {
  // A NULL is never a valid record or environment pointer, and physical address 0 is the BIOS /
  // KSEG-aliased region rather than a title-owned structure. Refusing it here is what makes "the
  // cell still holds zero" a named refusal instead of a wild access through the BIOS window.
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
    // The plan's guest draw width comes from the RECT.w the guest itself published. Graphical init
    // (0x80061140) runs before the render walk reaches 0x80075D0C, so this ordering is a property
    // of the title, not a hope — and if it ever stops holding, the honest answer is to stop, not to
    // substitute the window's own width and call it measured.
    refuse("projection publication ran before the guest published a draw environment");
  }

  const std::uint32_t record = core.mem_r32(kViewportRecordCell);
  if (!isGuestRam(record)) {
    lucent::error("spider1-wide",
                  "SLUS_008.75 published no viewport record at 0x{:08X} (cell holds 0x{:08X})",
                  kViewportRecordCell,
                  record);
    std::abort();
  }

  const std::uint16_t far = core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar);
  const std::uint16_t near = core.mem_r16(record + Spider1ViewportOffset::kHorizontalNear);
  const std::uint16_t centreX = core.mem_r16(record + Spider1ViewportOffset::kCentreX);

  // A uniform shift leaves (far - near) alone, so H stays bit-identical and the focal length needs
  // no correction at all. The lens divisor is therefore never read here and never written.
  const std::uint16_t span = fieldSpan(far, near);
  const std::uint16_t verticalSpan =
      fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kVerticalFar),
                core.mem_r16(record + Spider1ViewportOffset::kVerticalNear));
  const GuestProjectionPlan latched =
      relatch(core, measuredGeometry(span, verticalSpan, measuredDrawWidth_));
  appliedMargin_ = latched.projectionHorizontalMargin;
  if (!latched.widescreen()) {
    // 4:3 IDENTITY, with one case split out. A run that never widened writes NOTHING, so a 4:3
    // publication is byte-identical to retail by construction rather than by an argument that the
    // arithmetic happens to be neutral. A run that IS unwidening — the player changed the setting
    // back to 4:3 while a wide window is in the record — has to put the captured retail tuple back,
    // or the picture would keep a wide frustum inside a 4:3 canvas. Those are different operations
    // and the record's own state is what distinguishes them, so the branch is stated rather than
    // collapsed.
    if (retailCaptured_) {
      core.mem_w16(record + Spider1ViewportOffset::kHorizontalFar, retail_.horizontalFar);
      core.mem_w16(record + Spider1ViewportOffset::kHorizontalNear, retail_.horizontalNear);
      core.mem_w16(record + Spider1ViewportOffset::kCentreX, retail_.centreX);
    }
    retail(core);
    return;
  }

  // The margin is the SAME shift for both horizontal edges, and it is applied only where the record
  // does not already hold it. That is what makes the widening idempotent across the thousands of
  // publications in one run: this owner never adds its margin to a value it already widened, which
  // is exactly the cumulative 512 -> 684 -> 856 growth issue 0022 recorded.
  const std::uint32_t margin = static_cast<std::uint32_t>(latched.projectionHorizontalMargin);
  const auto shifted = [margin](std::uint16_t value) {
    return wrapField(static_cast<std::uint32_t>(value) + margin);
  };
  if (!retailCaptured_ || far != shifted(retail_.horizontalFar) ||
      near != shifted(retail_.horizontalNear)) {
    // Nothing is published yet, or the guest re-authored the window (an area or view change). Both
    // mean the values in the record RIGHT NOW are retail, and this is the moment to remember them.
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

  // What the OWNER must leave alone, sampled before the body runs. H is the title's own derivation
  // and the owner does not reimplement it; the guarantee it can make is narrower and exact: the
  // window's SPAN and the lens divisor are the only inputs that derivation reads for the focal
  // length, and both must come out the other side untouched. H is then compared against the value
  // the record carried for that same span, which is a comparison of inputs rather than a second
  // copy of the formula.
  const std::uint16_t retailCentreX = centreX;
  const std::uint16_t retailLens = core.mem_r16(record + Spider1ViewportOffset::kLensDivisor);
  const std::uint16_t distanceBefore =
      core.mem_r16(record + Spider1ViewportOffset::kScreenDistance);

  // Every write is retail + margin, NEVER the value just read. That is the idempotence: the record
  // holds the WIDENED window when the next publication reads it, so adding the margin to what was
  // read would grow 512 -> 684 -> 856 -> ... and republishing from the captured retail tuple
  // cannot. Only the HORIZONTAL pair moves: the record's depth window is a different axis, is
  // re-asserted by this very body, and a horizontal margin widens nothing in it.
  core.mem_w16(record + Spider1ViewportOffset::kHorizontalFar, shifted(retail_.horizontalFar));
  core.mem_w16(record + Spider1ViewportOffset::kHorizontalNear, shifted(retail_.horizontalNear));

  retail(core);

  // A uniform shift must not have changed what the focal length is derived from. If it did, the
  // frame is not the widening this owner claims and the run stops rather than presenting it.
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

  // The publication derives OFX, OFY and H itself, so the shifted window STAYS and the centre the
  // body produced is the widened one. That value is also what FUN_8007C2AC and FUN_8007B9CC
  // re-assert into CR24/CR25 from this record on every transformed vertex, which is why the record
  // rather than the libgte argument is the choke point a widening has to move.

  // The centre is the ONE value the owner does not write here: the title's own derivation produced
  // it from the widened window, and the plan's centre is the prediction that it did. A disagreement
  // means the two rules are not the same rule, and the frame would be something other than the
  // widening both describe.
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
  // H is unchanged whenever the span and the lens are, which the check above has already
  // established. Reporting the value rather than re-deriving it keeps this a report, not a second
  // implementation.
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

  // The body has written the whole RECT, so both the measured retail width and the height the plan
  // is measured against are the guest's own. Reading them here rather than from the argument
  // registers also means no stack-slot offset has to be guessed: 0x800884C0 takes RECT.h as a fifth
  // argument at a frame-dependent offset.
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
  // One precondition, and it is a fact about the GUEST rather than about the host: the draw
  // environment has been published, so the plan has a measured guest draw width to widen. Nothing
  // here needs the projection publication to have run — the host canvas this re-latch exists to
  // repair is derived by the framework from the live GP1 display width, not from the guest's
  // window — and requiring it would leave the canvas stale for exactly the frames between
  // graphical init and the first publication.
  if (!drawWidthMeasured()) {
    return;
  }
  const std::uint32_t record = core.mem_r32(kViewportRecordCell);
  if (!isGuestRam(record)) {
    // The publication itself installs this cell (0x80075DB0), so before the first one it does not
    // name a record. The plan latched at the draw-environment site stands until it does.
    return;
  }
  // Measured from the record as it stands. A uniform shift leaves the span untouched, so this is
  // the same span whether the window is currently retail or already widened.
  const GuestProjectionPlan latched = relatch(
      core,
      measuredGeometry(fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kHorizontalFar),
                                 core.mem_r16(record + Spider1ViewportOffset::kHorizontalNear)),
                       fieldSpan(core.mem_r16(record + Spider1ViewportOffset::kVerticalFar),
                                 core.mem_r16(record + Spider1ViewportOffset::kVerticalNear)),
                       measuredDrawWidth_));
  if (appliedMargin_ == latched.projectionHorizontalMargin) {
    // The required projection has not moved, so the guest record already says what this plan
    // demands. The HOST extent may still have changed — that is the whole reason this runs every
    // frame — and re-latching above has already published it.
    return;
  }
  appliedMargin_ = latched.projectionHorizontalMargin;
  if (!retailCaptured_) {
    // The guest has published a window but this owner has not widened one yet, so there is no
    // retail baseline to write from. The plan above is already correct for the live display extent
    // and the publication will widen the record the next time it runs; inventing a window here
    // would be the substitute-the-measured-width mistake this owner refuses everywhere else.
    return;
  }
  if (latched.widescreen()) {
    // Widening from a narrow canvas into a wide one, with no guest call: the record is the title's
    // own state and the values below are exactly what the publication's own derivation produces
    // from this window, which is why the next publication re-derives them unchanged.
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

} // namespace spider
