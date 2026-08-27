#include "spider1_mode_driver.h"

#include <array>
#include <cstdio>

int main() {
  using enum spider::Spider1OuterRoute;
  constexpr std::array expected{
      Invalid,
      RestartWithModeFive,
      Menu,
      Level,
      CycleSelection,
      CycleSelection,
      ResourcePrimary,
      TransitionThenOuter,
      ResetThenPrimary,
      Menu,
      TransitionThenOuterWithFlag,
      Invalid,
  };

  for (uint32_t selector = 0; selector < expected.size(); ++selector) {
    if (spider::spider1OuterRoute(selector) != expected[selector]) {
      std::fprintf(
          stderr, "outer selector %u did not follow the authenticated jump table\n", selector);
      return 1;
    }
  }

  std::puts("Spider1 outer selector: all ten retail routes and both invalid bounds agree");
  return 0;
}
