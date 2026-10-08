#include "spider1_movie_execution.h"

#include "guest_execution.h"

namespace spider::spider1 {
namespace {
constexpr std::uint32_t kRetailMoviePlayer = 0x8002AA0Cu;
}

psx::cpu::ExecutionResult Spider1MovieExecution::resume(Core &core) const {
  return GuestExecution(core).callOriginal(kRetailMoviePlayer);
}

} // namespace spider::spider1
