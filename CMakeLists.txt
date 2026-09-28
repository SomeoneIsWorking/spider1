cmake_minimum_required(VERSION 3.21)
project(spider_ports CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PSXPORT_DIR "${CMAKE_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR
    "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake; resolve the pinned psxport checkout")
endif()

execute_process(
  COMMAND git -C "${PSXPORT_DIR}" rev-parse HEAD
  OUTPUT_VARIABLE PSXPORT_RESOLVED_SHA
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_VARIABLE PSXPORT_REVISION_ERROR
  RESULT_VARIABLE PSXPORT_REVISION_RESULT)
if(NOT PSXPORT_REVISION_RESULT EQUAL 0)
  message(FATAL_ERROR
    "cannot identify PSXPORT_DIR=${PSXPORT_DIR}: ${PSXPORT_REVISION_ERROR}")
endif()
file(WRITE "${CMAKE_BINARY_DIR}/psxport_resolved.txt"
  "dir = ${PSXPORT_DIR}\ncommit = ${PSXPORT_RESOLVED_SHA}\n")
message(STATUS "psxport framework: ${PSXPORT_DIR} at ${PSXPORT_RESOLVED_SHA}")

find_package(ZLIB REQUIRED)
find_package(zstd CONFIG QUIET)
if(NOT TARGET zstd::libzstd_shared)
  find_library(ZSTD_LIBRARY zstd REQUIRED)
  add_library(zstd::libzstd_shared SHARED IMPORTED)
  set_target_properties(zstd::libzstd_shared PROPERTIES IMPORTED_LOCATION "${ZSTD_LIBRARY}")
endif()
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

add_subdirectory(${PSXPORT_DIR}/vendor/beetle-psx/deps/libchdr
                 ${CMAKE_BINARY_DIR}/libchdr_build EXCLUDE_FROM_ALL)
include(${PSXPORT_DIR}/cmake/psxport.cmake)
include(cmake/title_manifest.cmake)

set(SPIDER_COMMON_SOURCES
  game/core/executable_identity.cpp
  game/core/guest_execution.cpp
  game/core/spider_port.cpp
  game/core/spider_runtime.cpp)

set(SPIDER1_FRAME_SOURCES
  titles/spiderman1/spider1_stream_driver.cpp
  titles/spiderman1/spider1_movie_execution.cpp
  titles/spiderman1/spider1_frame_driver.cpp
  # The outer selector and the primary mode, plus the retail mode functions the mode driver composes
  # rather than absorbs: the title menu, the alternate mode, the invalid-selector input wait, the
  # display-field boundary all three modes share, the 3D transition's own arithmetic, the display
  # field clock VSync(0) is defined in terms of, ResetGraph's GPU half, and the CdInit body.
  titles/spiderman1/spider1_cd_initialization.cpp
  # The polled CD-ROM service 0x8008C3E0, recovered from the image: register reads, bounded
  # response drain, the five-arm dispatch and the status triplet. See spider1_cd_stream.h for the
  # byte evidence and for the correction to the "gate word" explanation this file's first draft
  # gave.
  titles/spiderman1/spider1_cd_stream.cpp
  titles/spiderman1/spider1_field_clock.cpp
  titles/spiderman1/spider1_gpu_reset.cpp
  titles/spiderman1/spider1_host_stepped_fiber.cpp
  titles/spiderman1/spider1_mode_driver.cpp
  titles/spiderman1/spider1_alternate_mode.cpp
  titles/spiderman1/spider1_invalid_selector_input.cpp
  titles/spiderman1/spider1_menu_mode.cpp
  titles/spiderman1/spider1_mode_frame_boundary.cpp
  titles/spiderman1/spider1_transition_wipe.cpp)

# The guest-widescreen owner links into the product: the title runtime returns its policy and
# registers its two measured publication overrides, so a picture depends on it.
set(SPIDER1_WIDESCREEN_SOURCES titles/spiderman1/spider1_widescreen.cpp)

function(spider_add_runtime_target TARGET TITLE PREFIX MAIN RUNTIME)
  spider_read_title(${TITLE} ${PREFIX})
  add_executable(${TARGET} ${SPIDER_COMMON_SOURCES} ${MAIN} ${RUNTIME})
  target_include_directories(${TARGET} PRIVATE game/core "titles/${TITLE}")
  target_compile_definitions(${TARGET} PRIVATE
    SPIDER_TITLE_SERIAL="${${PREFIX}_SERIAL}"
    SPIDER_TITLE_DISC_ENV="${${PREFIX}_DISCENV}"
    SPIDER_TITLE_GUEST_EXE="${${PREFIX}_GUESTEXECUTABLE}"
    SPIDER_TITLE_EXECUTABLE_SIZE=${${PREFIX}_FILESIZE}
    SPIDER_TITLE_EXECUTABLE_SHA256="${${PREFIX}_EXECUTABLESHA256}")
  target_link_libraries(${TARGET} PRIVATE psxport OpenSSL::Crypto)
  set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endfunction()

set(SPIDER_TITLES "spiderman1" CACHE STRING "Spider title targets to configure")
foreach(SPIDER_TITLE IN LISTS SPIDER_TITLES)
  if(SPIDER_TITLE STREQUAL "spiderman1")
    spider_add_runtime_target(
      spiderman_port spiderman1 SPIDER1
      game/core/main.cpp titles/spiderman1/spider1_runtime.cpp)
    target_sources(spiderman_port PRIVATE ${SPIDER1_FRAME_SOURCES} ${SPIDER1_WIDESCREEN_SOURCES})
  elseif(SPIDER_TITLE STREQUAL "spiderman2")
    spider_add_runtime_target(
      enter_electro_port spiderman2 SPIDER2
      titles/spiderman2/main.cpp titles/spiderman2/enter_electro_runtime.cpp)
  else()
    message(FATAL_ERROR "unknown SPIDER_TITLES entry '${SPIDER_TITLE}'")
  endif()
endforeach()

# Keep the remaining render owners compile-checked while the runtime attaches them through measured
# title boundaries.

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  add_executable(spider1_widescreen_test
    tests/spider1_widescreen_test.cpp
    titles/spiderman1/spider1_widescreen.cpp)
  target_include_directories(spider1_widescreen_test PRIVATE
    game/core titles/spiderman1 ${PSXPORT_DIR}/tests)
  target_link_libraries(spider1_widescreen_test PRIVATE psxport)
  add_test(NAME spider1_widescreen COMMAND spider1_widescreen_test)

  add_executable(face_builder_census_test
    tests/face_builder_census_test.cpp
    game/render/face_builder_census.cpp)
  target_include_directories(face_builder_census_test PRIVATE game/render)
  add_test(NAME face_builder_census COMMAND face_builder_census_test)

  add_executable(mesh_transform_contract_test
    tests/mesh_transform_contract_test.cpp
    game/render/mesh_transform.cpp)
  target_include_directories(mesh_transform_contract_test PRIVATE game/render)
  add_test(NAME mesh_transform_contract COMMAND mesh_transform_contract_test)

  add_executable(mesh_animated_vertex_test
    tests/mesh_animated_vertex_test.cpp
    game/render/mesh_animated_vertex.cpp
    game/render/mips_fixed_point.cpp)
  target_include_directories(mesh_animated_vertex_test PRIVATE game/render)
  add_test(NAME mesh_animated_vertex COMMAND mesh_animated_vertex_test)

  add_executable(mesh_pose_contract_test
    tests/mesh_pose_contract_test.cpp
    game/render/mesh_pose_contract.cpp
    game/render/mips_fixed_point.cpp)
  target_include_directories(mesh_pose_contract_test PRIVATE game/render)
  add_test(NAME mesh_pose_contract COMMAND mesh_pose_contract_test)

  add_executable(asset_upload_ledger_test
    tests/asset_upload_ledger_test.cpp
    game/render/asset_upload_ledger.cpp)
  target_include_directories(asset_upload_ledger_test PRIVATE game/render)
  add_test(NAME asset_upload_ledger COMMAND asset_upload_ledger_test)

  add_executable(mesh_asset_cook_test
    tests/mesh_asset_cook_test.cpp
    game/render/mesh_asset_cook.cpp)
  target_include_directories(mesh_asset_cook_test PRIVATE game/render)
  add_test(NAME mesh_asset_cook COMMAND mesh_asset_cook_test)

  add_executable(spider1_mode_transition_test tests/spider1_mode_transition_test.cpp)
  target_include_directories(spider1_mode_transition_test PRIVATE titles/spiderman1)
  add_test(NAME spider1_mode_transition COMMAND spider1_mode_transition_test)

  add_executable(spider1_field_schedule_test tests/spider1_field_schedule_test.cpp)
  target_include_directories(spider1_field_schedule_test PRIVATE titles/spiderman1)
  add_test(NAME spider1_field_schedule COMMAND spider1_field_schedule_test)

  # The mode drivers' DECISIONS, with no Core at all. Every rule they were extracted around is a
  # constexpr function of what the mode observed, so its whole input space is enumerable here — which
  # is what makes these the tests that can catch a readability refactor changing a rule rather than
  # renaming it. Each rule is pinned from both sides, so a rule that always answered one way fails.
  # The wipe is linked because its per-pixel darkening is a rule, and the test exercises the SHIPPING
  # function rather than a second copy of the arithmetic beside it.
  add_executable(spider1_mode_rules_test
    tests/spider1_mode_rules_test.cpp titles/spiderman1/spider1_transition_wipe.cpp)
  target_include_directories(spider1_mode_rules_test PRIVATE titles/spiderman1 ${PSXPORT_DIR}/tests)
  target_link_libraries(spider1_mode_rules_test PRIVATE psxport)
  add_test(NAME spider1_mode_rules COMMAND spider1_mode_rules_test)

  # The guest-boundary owners, driven against a real Core with native-override probes standing in for
  # the retail guest bodies. The probes record the call ORDER as well as the count, so a reordering
  # fails here even when the game would still run, and every probe reports whether it was entered at
  # an address it was installed for.
  add_executable(spider1_guest_boundary_owners_test
    tests/spider1_guest_boundary_owners_test.cpp
    titles/spiderman1/spider1_cd_initialization.cpp
    titles/spiderman1/spider1_field_clock.cpp
    titles/spiderman1/spider1_gpu_reset.cpp
    titles/spiderman1/spider1_host_stepped_fiber.cpp
    titles/spiderman1/spider1_invalid_selector_input.cpp
    titles/spiderman1/spider1_mode_frame_boundary.cpp)
  target_include_directories(spider1_guest_boundary_owners_test
    PRIVATE game/core titles/spiderman1 ${PSXPORT_DIR}/tests)
  target_link_libraries(spider1_guest_boundary_owners_test PRIVATE psxport)
  add_test(NAME spider1_guest_boundary_owners COMMAND spider1_guest_boundary_owners_test)

  add_executable(executable_identity_test
    tests/executable_identity_test.cpp game/core/executable_identity.cpp)
  target_include_directories(executable_identity_test PRIVATE game/core)
  target_link_libraries(executable_identity_test PRIVATE OpenSSL::Crypto)
  add_test(NAME executable_identity COMMAND executable_identity_test)
  add_test(NAME title_catalog
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_title_catalog.py)
  add_test(NAME executable_provision
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_provision.py)
  add_test(NAME launcher_policy
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/run.py --selftest)
  add_test(NAME ci_workflow
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_ci_workflow.py)
  add_test(NAME source_policy
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/source_policy.py)
  add_test(NAME source_policy_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/source_policy.py --selftest)
  # The cull census classifies instruction words, and its classifier's correctness IS the coverage
  # claim the widescreen owner cites in its header ("all 17 classified functions take their bound
  # from the record"). An ungated classifier rots silently: this decoder got the GTE encoding wrong
  # three separate ways while it was being written — `rtps` (funct 0x16) matched nothing, the
  # `rt` bits 10..6 matched nothing, and `& 0xFFF` yielded a plausible 38 that skipped every CR with
  # bit 0 set — and the self-test caught all three. So the self-test is the gate, not a nicety.
  add_test(NAME spider1_cull_census_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/probe_cull_census.py --self-test)
  # The main-RAM-global bound census, for the same reason and with the same class of failure in
  # mind. `probe_cull_census.py` counts references to the viewport-record cell and, in its own
  # closing line, reports 372 `addiu`/`ori` sites loading a 4:3-derived literal while saying it
  # cannot decide whether any of them bounds a projected coordinate. That silence is exactly what
  # Crash 1 read as "widening cannot clip new geometry", and it was wrong: the bound there was a
  # main-RAM GLOBAL (1 writer, 20 readers) that no literal scan can see, used as the GTE NEAR
  # PLANE. So the question has its own instrument, and the instrument's classifier is gated for
  # the reason the line above states.
  add_test(NAME spider1_global_bounds_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/probe_global_bounds.py --selftest)
  # The 4:3-vs-16:9 receipt logic. The selftest half is a ctest test because it needs no disc, no GPU
  # and no product; the pair itself is a maintainer run (it needs both a provisioned image and the
  # machine's single product slot). The negative case is the point of it: the same code that reads
  # `render_width=428 > native_width=320` as widened reads a 4:3 log as NOT widened, and a log with
  # no `[wide]` line refuses instead of answering. See docs/issues/0025.
  add_test(NAME spider1_widescreen_pair_selftest
    COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_SOURCE_DIR}/tools/probe_spider1_widescreen_pair.py --selftest)
  # The BIOS-call census, read from the image's own stub bytes rather than from a table somebody
  # remembered. It is the instrument that answers "which BIOS service is this title missing", which
  # is a hard stop: `dispatchGuestHostService` returns a Fault with reason `unimplemented BIOS
  # service`. Its selftest pins the trap this census fell into once — a `jr` recognised by testing
  # bits 4..0, which are the funct field's own low bits, so 100% of real stubs were rejected and the
  # census reported ZERO BIOS calls, the answer that reads as "this title makes none". See
  # docs/issues/0026.
  add_test(NAME spider1_bios_stub_census_selftest
    COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_SOURCE_DIR}/tools/probe_bios_stub_census.py --selftest)
  # Spider-Man 1's per-frame field count, which decides whether an interpolated 60 fps path is IN
  # scope for this title. The ANSWER is deliberately "not established": the frame body waits
  # through its own FUN_8005E748 rather than VSync, and one of its two wait sites sits inside a
  # back-edge loop whose trip count a DrawSync(1) GPU fence decides at run time, so the per-frame
  # field count is not a compile-time constant. The instrument is registered precisely because
  # that is the answer: a cadence tool that must print a number will print a wrong one, and this
  # title is the case the workspace nearly ruled out of scope on a docstring. Its selftest
  # carries a DISCRIMINATOR -- the same tool on the same bytes with the loop removed MUST report
  # a definite rate -- so the refusal cannot be confused with a broken counter. Both halves read
  # the provisioned image, which hosted CI does not have, so both exit 2 with a REFUSAL rather
  # than reporting a rate.
  add_test(NAME spider1_cadence_selftest
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/re_cadence.py --selftest)
  add_test(NAME spider1_cadence
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/re_cadence.py)
  # The polled CD-ROM service 0x8008C3E0's recovery, and the gate census that decides what may be
  # done about the word 0x800B2886. Registered because the constants in
  # titles/spiderman1/spider1_cd_stream.h are only as good as this scan: the tool asserts every one
  # of them against the authenticated image, and its selftest mutates the dispatch table, the gate
  # halfword, the SETTER census, the set of gate-reader call sites, a call site and a result bit, so
  # "there is no setter" is falsifiable rather than an absence of a scan. Its positive control --
  # the unmutated image firing no assertion -- is what makes the seven red cases meaningful. Both
  # halves need the provisioned image, which hosted CI does not have, so both REFUSE (exit 2)
  # instead of reporting a recovery.
  add_test(NAME spider1_cd_stream_selftest
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/re_cd_stream.py --selftest)
  add_test(NAME spider1_cd_stream
    COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_SOURCE_DIR}/tools/re_cd_stream.py)
  # The bounded headless run's decoders: the I_STAT bit names, the CD status bit names, the
  # control-surface data-line parser, and the refusal an empty sample set must produce. Those are the
  # pieces that turn a register read into a sentence, and a wrong bit table is a confidently wrong
  # sentence. The run itself needs a disc and the machine's single product slot, so it is a
  # maintainer command, not a ctest case.
  add_test(NAME spider1_headless_run_selftest
    COMMAND ${Python3_EXECUTABLE} -B
            ${CMAKE_SOURCE_DIR}/tools/probe_spider1_headless_run.py --selftest)
  # The CD-ROM interrupt's ownership contract, from the guest's bytes. It carries a BASELINE
  # assertion that is EXPECTED TO FAIL once the issue-0026 framework change lands, and inverting it is
  # then the regression test for that fix. It is here so the evidence outlasts the session, and so
  # the finding cannot be re-derived a fourth time from a log line. See docs/issues/0026.
  add_executable(cd_irq2_delivery_test tests/cd_irq2_delivery_test.cpp)
  target_link_libraries(cd_irq2_delivery_test PRIVATE psxport)
  add_test(NAME cd_irq2_delivery COMMAND cd_irq2_delivery_test)
  if(TARGET spiderman_port)
    spider_add_runtime_target(
      spider1_runtime_services_test spiderman1 SPIDER1
      tests/spider1_runtime_services_test.cpp titles/spiderman1/spider1_runtime.cpp)
    target_sources(spider1_runtime_services_test PRIVATE
      ${SPIDER1_FRAME_SOURCES} ${SPIDER1_WIDESCREEN_SOURCES})
    target_include_directories(spider1_runtime_services_test PRIVATE ${PSXPORT_DIR}/tests)
    add_test(NAME spider1_runtime_services COMMAND spider1_runtime_services_test)
    set_tests_properties(spider1_runtime_services PROPERTIES ENVIRONMENT
      "PSXPORT_NOWINDOW=1;PSXPORT_NOAUDIO=1;PSXPORT_NOPACE=1;PSXPORT_ASSET_DIR=${PSXPORT_DIR}")
    add_test(NAME launcher_help
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_help_contract.py
              --executable $<TARGET_FILE:spiderman_port>)
  endif()
  add_test(NAME psxport_pin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/psxport_sync.py
            --check --build ${CMAKE_BINARY_DIR})
  add_test(NAME cpp_policy
    COMMAND ${Python3_EXECUTABLE} ${PSXPORT_DIR}/tools/check_cpp_style.py
            --root ${CMAKE_SOURCE_DIR} --compile-commands ${CMAKE_BINARY_DIR}
            # Shrink-only caps, ratcheted to the exact size each guest driver reached when its retail
            # mode functions, its field clock, and its host-stepped fiber were extracted into their own
            # owners (2026-09-28). Neither was capped before: both sat under the 1,200-line default, so
            # there was no legacy list to come off. A cap here is a TIGHTENING, never a raise — the
            # gate fails a file that shrank while keeping its cap, so each of these has to be lowered
            # again in the same change that makes the file smaller.
            --cap titles/spiderman1/spider1_frame_driver.cpp=573
            --cap titles/spiderman1/spider1_mode_driver.cpp=559)
endif()

# THE LIVE PIN CHECK. Not a selftest: this runs the real function against this repository's real
# psxport.pin and the real build receipt, so it can fail when the pin is actually wrong.
#
# MEASURED 2026-09-27: every port in the workspace registered only a SELFTEST of the check function, run
# against a temporary fixture. A selftest asserts the function behaves; it cannot fail when this repo's
# pin is stale, which is why ten ports reported green while four pins were demonstrably stale. --build
# points at THIS build directory, configured moments ago, so the receipt is current by construction and
# the staleness guard cannot fire spuriously. Verified: perturbing psxport.pin turns it red.
if(BUILD_TESTING)
  add_test(
    NAME spider1_psxport_pin_live
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/psxport_sync.py"
            --check --build "${CMAKE_BINARY_DIR}")
endif()

# The pin tool's own behaviour: the staleness guard, and a bump that records the commit the build
# RESOLVED rather than the framework's current HEAD. Added because 7 of 10 ports shipped the tool with no
# test gating it at all. Unguarded on purpose where this repository does not define BUILD_TESTING -- a
# guarded add_test there is silently inert, which is the failure this gate exists to prevent.
add_test(
  NAME spider1_psxport_pin_selftest
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_psxport_sync.py")
