# cmake/spiderman_port.cmake — builds the native port binary `spiderman_port`.
#
# The target compiles ONLY game/* (the framework seam) + generated/* (the recompiled substrate
# shards) and links libpsxport.a, which carries all PSX-generic framework code and its system deps.
#
#   cmake -S . -B build && cmake --build build --target spiderman_port
#   ./scratch/bin/spiderman_port scratch/bin/spiderman/SLUS_008.75

option(PSXPORT_BUILD_PORT "Build the Spider-Man native port binary (spiderman_port)" ON)

# The framework static library + its psxport_smoke agnosticism proof. Always included so `psxport`
# and `psxport_smoke` stay buildable even when the game target is off.
include(${CMAKE_SOURCE_DIR}/external/psxport/cmake/psxport.cmake)

if(NOT PSXPORT_BUILD_PORT)
  return()
endif()

# ---- game source list -------------------------------------------------------------------------
# Phase 0: the seam only. No game behaviour is owned natively yet — every guest function runs on the
# substrate. Ported subsystems get added here as they land (see docs/codemap.md).
set(GAME_SRC
  game/core/main.cpp              # process entry point
  game/core/game_config.cpp       # the RE'd GameConfig (guest address literals)
  game/core/game_hooks.cpp        # the GameHooks vtable
  game/core/recomp_register.cpp   # the generated-substrate seam
  game/core/sync_native.cpp       # RE'd PSX hardware-sync primitives (libetc VSync)
  game/core/cd_stream.cpp         # continuous-read (XA/STR) pump, driven from StGetNext
  game/core/diag_overrides.cpp)   # observe-only overrides (log + super-call), channel-gated

# ---- the recompiled substrate -----------------------------------------------------------------
# emit.py writes the exact TU list to generated/rec_sources.cmake (GEN_REC_SRCS, basenames).
# Compiled as C++ despite the .c extension, matching the recompiler's output language.
#
# -foptimize-sibling-calls is REQUIRED, not an optimization nicety: a guest TAIL JUMP (a computed
# `jr` routed to rec_dispatch, or a branch to a framed sibling) is emitted as `dispatch(c,x); return;`
# in tail position, and guests use such tail jumps for unbounded state-machine LOOPS. Without sibling-
# call optimization every iteration is a real C call, the stack grows per iteration, and the run
# SIGSEGVs. -O2 enables it; it is set explicitly atop -O1 so the dependency is documented and
# survives an -O level change.
include(${CMAKE_SOURCE_DIR}/generated/rec_sources.cmake)
list(TRANSFORM GEN_REC_SRCS PREPEND generated/)
set_source_files_properties(${GEN_REC_SRCS}
  PROPERTIES LANGUAGE CXX
  COMPILE_OPTIONS "-O1;-foptimize-sibling-calls;-fno-strict-aliasing;-fwrapv")

add_executable(spiderman_port ${GAME_SRC} ${GEN_REC_SRCS})

# The framework's SDL_GPU shader header is produced by a psxport custom target; the game exe needs it
# present before its own compile ordering.
add_dependencies(spiderman_port gen_gpu_shaders)

set_target_properties(spiderman_port PROPERTIES
  CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON
  ENABLE_EXPORTS ON                                   # -rdynamic: watchdog backtrace symbol names
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/scratch/bin)

# Framework include dirs (runtime, generated, vendored backends, SDL/freetype) come PUBLICly from the
# psxport link; only the game's own subfolders are added here.
target_include_directories(spiderman_port PRIVATE game game/core)

target_compile_options(spiderman_port PRIVATE -w -O2 -g
  ${SDL3_CFLAGS_OTHER} ${FREETYPE_CFLAGS_OTHER})

target_link_libraries(spiderman_port PRIVATE psxport)
