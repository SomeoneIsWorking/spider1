#include "spider1_field_schedule.h"

#include <cstdio>

int main() {
  const auto initialBoot = spider::spider1PlanFiberResume(true, false);
  const auto carriedSatisfied = spider::spider1PlanFiberResume(true, true);
  const auto missingBoundary = spider::spider1PlanFiberResume(false, false);
  const auto firstYield = spider::spider1PlanFiberYield(true, false, false, false);
  const auto secondYieldSameStep = spider::spider1PlanFiberYield(true, false, true, false);
  const auto cleanReturn = spider::spider1PlanFiberYield(false, false, true, true);
  const auto duplicateFence = spider::spider1PlanFiberYield(true, false, false, true);

  if (!initialBoot.valid || !initialBoot.deliverField || !initialBoot.resume ||
      !carriedSatisfied.valid || carriedSatisfied.deliverField || !carriedSatisfied.resume ||
      missingBoundary.valid || !firstYield.valid || !firstYield.deliverField ||
      !firstYield.fieldSatisfied || !firstYield.commit || !secondYieldSameStep.valid ||
      secondYieldSameStep.deliverField || secondYieldSameStep.fieldSatisfied ||
      !secondYieldSameStep.commit || !cleanReturn.valid || cleanReturn.commit ||
      duplicateFence.valid) {
    std::puts("Spider1 finite-fiber field scheduling contract failed");
    return 1;
  }
  std::puts("Spider1 finite fiber: guest work runs between single-field host fences");
  return 0;
}
