# cmake/spiderman_port.cmake — builds the native port binary `spiderman_port`.
#
# The target compiles ONLY game/* (the framework seam) + generated/* (the recompiled substrate
# shards) and links libpsxport.a, which carries all PSX-generic framework code and its system deps.
#
#   cmake -S . -B build && cmake --build build --target spiderman_port
#   ./scratch/bin/spiderman_port scratch/bin/spiderman/SLUS_008.75

option(PSXPORT_BUILD_PORT "Build the Spider-Man native port binary (spiderman_port)" ON)
option(SPIDER_BUILD_IRQ_POLL_AUDIT
       "Link the full-R3000 deferred-work preservation discriminator" OFF)

if(NOT PSXPORT_BUILD_PORT)
  return()
endif()

spider_read_title(spiderman1 SPIDER1)

# The proprietary executable-derived body stays outside the repository. Derive the resumable STR
# override from the user's locally generated substrate, validating the three authenticated VSync
# return PCs before compiling it. The original gen_func_8002AA0C remains in GEN_REC_SRCS as its
# runtime super/oracle.
set(SPIDER1_MOVIE_FIBER_SOURCE
  "${CMAKE_BINARY_DIR}/generated/spider1_movie_body.cpp")
add_custom_command(
  OUTPUT "${SPIDER1_MOVIE_FIBER_SOURCE}"
  COMMAND
    "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/generate_spider1_movie_fiber.py"
    --input "${CMAKE_SOURCE_DIR}/generated/shard_6.c"
    --output "${SPIDER1_MOVIE_FIBER_SOURCE}"
  DEPENDS
    "${CMAKE_SOURCE_DIR}/tools/generate_spider1_movie_fiber.py"
    "${CMAKE_SOURCE_DIR}/generated/shard_6.c"
  VERBATIM)

# ---- game source list -------------------------------------------------------------------------
# Phase 0: the seam only. No game behaviour is owned natively yet — every guest function runs on the
# substrate. Ported subsystems get added here as they land (see docs/codemap.md).
set(GAME_SRC
  game/core/main.cpp              # process entry point
  game/core/spider_port.cpp       # shared process boot composition
  game/core/executable_identity.cpp  # serial + byte identity before Game construction
  game/core/spider_runtime.cpp    # address-free lineage runtime base
  titles/spiderman1/spider1_runtime.cpp  # title behavior and legacy migration boundary
  titles/spiderman1/spider1_frame_driver.cpp  # finite boot + cadence/service ownership
  titles/spiderman1/spider1_mode_driver.cpp  # finite outer selector and subordinate mode states
  titles/spiderman1/spider1_widescreen.cpp  # title-owned guest projection/culling publication
  game/core/spider_context.cpp    # per-Core Spider runtime subsystem aggregate
  game/core/allocator_audit.cpp   # opt-in retail allocator first-corruption discriminator
  game/core/irq_poll_audit.cpp    # compile-time full-R3000 deferred-work discriminator
  game/core/game_config.cpp       # measured legacy address facts awaiting typed interfaces
  game/core/game_hooks.cpp        # bounded compatibility callbacks awaiting typed interfaces
  game/core/recomp_register.cpp   # the generated-substrate seam
  game/core/cd_stream.cpp         # continuous-read (XA/STR) pump, driven from StGetNext
  game/core/diag_overrides.cpp    # observe-only overrides (log + super-call), channel-gated
  game/core/str_skip_oracle.cpp   # opt-in retail STR skip discriminator + selftest
  game/core/module_loader.cpp     # pins runtime-loaded CD.WAD modules to one canonical slot (RE-09)
  game/render/scene_id.cpp        # the game's own level-name -> scene-id lens (RE-23)
  game/render/frame_census.cpp    # RE-21 display-list inventory instrument (diagnostic only)
  game/render/face_builder_census.cpp  # RE-21 exact FUN_8007C4D8 input-owner census
  game/render/mesh_face_format.cpp  # RE-21 executable-derived source face semantics
  game/render/mesh_transform.cpp  # RE-21 object-local-to-camera source transform contract
  game/render/mesh_animated_vertex.cpp  # RE-21 animated source/cache staging contract
  game/render/mesh_pose_contract.cpp  # RE-21 pre-GTE animated pose-composition contract
  game/render/mips_fixed_point.cpp  # shared exact signed/shift semantics for render source records
  game/render/asset_upload_ledger.cpp  # RE-21 authored texture/CLUT ownership ledger
  game/render/mesh_asset_cook.cpp  # RE-21 retained load-time cooked face ownership
  game/render/texture_asset_probe.cpp  # RE-21 retail asset/upload/lifetime instrument
  game/render/mesh_probe.cpp      # RE-21 mesh/object submission-boundary instrument
  game/render/gpu_env.cpp         # DRAWENV/DISPENV lenses + the ported libgpu word builders
  game/render/frame_envelope.cpp  # THE FIRST NATIVE PRODUCER: page flip + draw area + clear
  game/render/guest_frame_fallback.cpp  # HACK-03 mutually exclusive guest-frame safety gate
  game/render/render_seam.cpp     # the render seam on the engine's submitFrame (RE-20)
  ${SPIDER1_MOVIE_FIBER_SOURCE})  # executable-derived STR body with native field-yield seams

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
  COMPILE_OPTIONS "-w;-O1;-foptimize-sibling-calls;-fno-strict-aliasing;-fwrapv")
set_source_files_properties("${SPIDER1_MOVIE_FIBER_SOURCE}"
  PROPERTIES GENERATED TRUE LANGUAGE CXX
  COMPILE_OPTIONS "-w;-O1;-fno-strict-aliasing;-fwrapv")

add_executable(spiderman_port ${GAME_SRC} ${GEN_REC_SRCS})

spider_configure_target(spiderman_port SPIDER1)
target_include_directories(spiderman_port PRIVATE titles/spiderman1)

if(SPIDER_BUILD_IRQ_POLL_AUDIT)
  target_compile_definitions(spiderman_port PRIVATE SPIDER_IRQ_POLL_AUDIT_ENABLED=1)
  target_link_options(spiderman_port PRIVATE -Wl,--wrap=rec_irq_poll)
endif()
