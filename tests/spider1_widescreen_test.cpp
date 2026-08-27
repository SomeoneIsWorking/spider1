#include "spider1_widescreen.h"

#include <cstdio>

void gen_func_80075D0C(Core *) {}

int main() {
  using spider::spider1ProjectViewport;
  using spider::Spider1ViewportHorizontal;

  const Spider1ViewportHorizontal retail{.left = 512, .right = 0, .lensDivisor = 2365};
  if (!(spider1ProjectViewport(retail, 512) == retail)) {
    std::fputs("native viewport did not remain byte-identical\n", stderr);
    return 1;
  }
  const Spider1ViewportHorizontal wide = spider1ProjectViewport(retail, 684);
  if (wide.left != 684 || wide.right != 0 || wide.lensDivisor != 3159) {
    std::fprintf(stderr,
                 "16:9 viewport mismatch: left=%u right=%u lens=%u\n",
                 wide.left,
                 wide.right,
                 wide.lensDivisor);
    return 1;
  }

  const Spider1ViewportHorizontal reversed{.left = 20, .right = 340, .lensDivisor = 1600};
  const Spider1ViewportHorizontal reversedWide = spider1ProjectViewport(reversed, 428);
  if (reversedWide.left != 20 || reversedWide.right != 448 || reversedWide.lensDivisor != 2140) {
    std::fputs("opposite viewport orientation did not preserve its anchor or scale\n", stderr);
    return 1;
  }

  std::puts("Spider-Man 1 widescreen: viewport and lens scale together for both orientations");
  return 0;
}
