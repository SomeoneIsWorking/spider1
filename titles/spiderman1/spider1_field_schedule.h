#pragma once

namespace spider {

struct Spider1FiberResumePlan {
  bool valid = false;
  bool deliverField = false;
  bool resume = false;
};

// A carried fiber is blocked at a title field boundary. If the previous host step already delivered
// that field, resume immediately. Otherwise deliver it and resume in this same step so guest work
// runs before presentation pacing; deferring the resume until the next paced step starves the
// guest.
constexpr Spider1FiberResumePlan spider1PlanFiberResume(bool waiting, bool fieldSatisfied) {
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

// A newly reached yield needs one presentation fence. It may consume a field only if this host step
// has not already delivered one; otherwise the wait remains unsatisfied for the next step.
constexpr Spider1FiberYieldPlan
spider1PlanFiberYield(bool waiting, bool fieldSatisfied, bool deliveredField, bool committed) {
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

} // namespace spider
