#include "spider1_widescreen.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <lucent/log.h>

namespace spider {
namespace {

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

} // namespace spider
