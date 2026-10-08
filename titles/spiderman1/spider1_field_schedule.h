#pragma once

namespace spider::spider1 {

struct Spider1FiberResumePlan {
  bool valid = false;
  bool deliverField = false;
  bool resume = false;
};

// A carried fiber blocked at a field boundary: deliver the field unless already delivered, then
// resume in this step so guest work runs before presentation pacing.
constexpr Spider1FiberResumePlan planFiberResume(bool waiting, bool fieldSatisfied) {
  return {
      .valid = waiting,
      .deliverField = waiting && !fieldSatisfied,
      .resume = waiting,
  };
}

struct Spider1FiberYieldPlan {
  bool valid = false;
  bool deliverField = false;
  bool fieldSatisfied = false;
  bool commit = false;
};

// A new yield takes one fence and consumes a field only if this step delivered none.
constexpr Spider1FiberYieldPlan
planFiberYield(bool waiting, bool fieldSatisfied, bool deliveredField, bool committed) {
  if (!waiting) {
    return {.valid = true};
  }
  if (fieldSatisfied || committed) {
    return {};
  }
  return {
      .valid = true,
      .deliverField = !deliveredField,
      .fieldSatisfied = !deliveredField,
      .commit = true,
  };
}

} // namespace spider::spider1
