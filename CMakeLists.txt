cmake_minimum_required(VERSION 3.21)
project(spider_ports CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PSXPORT_DIR "${CMAKE_SOURCE_DIR}/external/psxport" CACHE PATH
    "psxport framework source directory")
if(NOT EXISTS "${PSXPORT_DIR}/cmake/psxport.cmake")
  message(FATAL_ERROR
    "PSXPORT_DIR=${PSXPORT_DIR} has no cmake/psxport.cmake; run tools/psxport_fetch.py")
endif()
message(STATUS "psxport framework: ${PSXPORT_DIR}")

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
  titles/spiderman1/spider1_bootstrap_turn.cpp
  titles/spiderman1/spider1_stream_driver.cpp
  titles/spiderman1/spider1_movie_execution.cpp
  titles/spiderman1/spider1_frame_driver.cpp
  # Mode driver and the retail mode functions it composes.
  titles/spiderman1/spider1_cd_initialization.cpp
  titles/spiderman1/spider1_field_clock.cpp
  titles/spiderman1/spider1_gpu_reset.cpp
  titles/spiderman1/spider1_host_stepped_fiber.cpp
  titles/spiderman1/spider1_mode_driver.cpp
  titles/spiderman1/spider1_alternate_mode.cpp
  titles/spiderman1/spider1_invalid_selector_input.cpp
  titles/spiderman1/spider1_menu_mode.cpp
  titles/spiderman1/spider1_mode_frame_boundary.cpp
  titles/spiderman1/spider1_transition_wipe.cpp)

# Linked into the product: the title runtime returns its policy and registers two overrides.
set(SPIDER1_WIDESCREEN_SOURCES titles/spiderman1/spider1_widescreen.cpp)

function(spider_add_runtime_target TARGET TITLE PREFIX MAIN RUNTIME)
  spider_read_title(${TITLE} ${PREFIX})
  add_executable(${TARGET} ${SPIDER_COMMON_SOURCES} ${MAIN} ${RUNTIME})
  target_include_directories(${TARGET} PRIVATE game/core "titles/${TITLE}")
  target_compile_definitions(${TARGET} PRIVATE
    SPIDER_TITLE_ID="${${PREFIX}_ID}"
    SPIDER_TITLE_LABEL="${${PREFIX}_LABEL}"
    SPIDER_TITLE_SERIAL="${${PREFIX}_SERIAL}"
    SPIDER_TITLE_DISC_ENV="${${PREFIX}_DISCENV}"
    SPIDER_TITLE_GUEST_EXE="${${PREFIX}_GUESTEXECUTABLE}"
    SPIDER_TITLE_EXECUTABLE_SIZE=${${PREFIX}_FILESIZE}
    SPIDER_TITLE_EXECUTABLE_SHA256="${${PREFIX}_EXECUTABLESHA256}")
  if(DEFINED ${PREFIX}_HEADER_ENTRY)
    target_compile_definitions(${TARGET} PRIVATE
      SPIDER_TITLE_HEADER_ENTRY=${${PREFIX}_HEADER_ENTRY}u
      SPIDER_TITLE_HEADER_GP=${${PREFIX}_HEADER_GP}u
      SPIDER_TITLE_HEADER_TEXT_ADDRESS=${${PREFIX}_HEADER_TEXTADDRESS}u
      SPIDER_TITLE_HEADER_TEXT_SIZE=${${PREFIX}_HEADER_TEXTSIZE}u
      SPIDER_TITLE_HEADER_STACK_ADDRESS=${${PREFIX}_HEADER_STACKADDRESS}u
      SPIDER_TITLE_HEADER_STACK_OFFSET=${${PREFIX}_HEADER_STACKOFFSET}u)
  endif()
  target_link_libraries(${TARGET} PRIVATE psxport OpenSSL::Crypto)
  set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endfunction()

set(SPIDER_TITLES "spiderman1" CACHE STRING "Spider title targets to configure")
foreach(SPIDER_TITLE IN LISTS SPIDER_TITLES)
  if(SPIDER_TITLE STREQUAL "spiderman1")
    spider_add_runtime_target(
      spiderman_port spiderman1 SPIDER1
      game/core/main.cpp titles/spiderman1/spider1_runtime.cpp)
    target_sources(spiderman_port PRIVATE
      ${SPIDER1_FRAME_SOURCES} ${SPIDER1_WIDESCREEN_SOURCES} titles/spiderman1/spider1_catalog.cpp)
  elseif(SPIDER_TITLE STREQUAL "spiderman2")
    spider_add_runtime_target(
      enter_electro_port spiderman2 SPIDER2
      titles/spiderman2/main.cpp titles/spiderman2/enter_electro_runtime.cpp)
  else()
    message(FATAL_ERROR "unknown SPIDER_TITLES entry '${SPIDER_TITLE}'")
  endif()
endforeach()

# Render owners no product target links yet; built as one library so the contracts stay compiling.
add_library(spider_render_contracts STATIC
  game/render/frame_envelope.cpp
  game/render/gpu_env.cpp
  game/render/scene_id.cpp)
target_include_directories(spider_render_contracts PRIVATE game/render)
target_link_libraries(spider_render_contracts PRIVATE psxport)

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

  add_executable(mesh_asset_cook_test
    tests/mesh_asset_cook_test.cpp
    game/render/mesh_asset_cook.cpp)
  target_include_directories(mesh_asset_cook_test PRIVATE game/render)
  add_test(NAME mesh_asset_cook COMMAND mesh_asset_cook_test)

  add_executable(mesh_face_format_test
    tests/mesh_face_format_test.cpp
    game/render/mesh_face_format.cpp)
  target_include_directories(mesh_face_format_test PRIVATE game/render)
  add_test(NAME mesh_face_format COMMAND mesh_face_format_test)

  add_executable(spider1_mode_transition_test tests/spider1_mode_transition_test.cpp)
  target_include_directories(spider1_mode_transition_test PRIVATE titles/spiderman1)
  add_test(NAME spider1_mode_transition COMMAND spider1_mode_transition_test)

  add_executable(spider1_field_schedule_test tests/spider1_field_schedule_test.cpp)
  target_include_directories(spider1_field_schedule_test PRIVATE titles/spiderman1)
  add_test(NAME spider1_field_schedule COMMAND spider1_field_schedule_test)

  # Mode driver decisions, tested without a Core; links the wipe to test the shipping darkening rule.
  add_executable(spider1_mode_rules_test
    tests/spider1_mode_rules_test.cpp titles/spiderman1/spider1_transition_wipe.cpp)
  target_include_directories(spider1_mode_rules_test PRIVATE titles/spiderman1 ${PSXPORT_DIR}/tests)
  target_link_libraries(spider1_mode_rules_test PRIVATE psxport)
  add_test(NAME spider1_mode_rules COMMAND spider1_mode_rules_test)

  # Guest-boundary owners against a real Core with native-override probes that record call order.
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
  add_test(NAME source_policy
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/source_policy.py)
  # CD-ROM interrupt ownership contract; its baseline assertion is expected to fail once issue 0026 lands (docs/issues/0026).
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
    spider_add_runtime_target(
      spider1_catalog_test spiderman1 SPIDER1
      tests/spider1_catalog_test.cpp titles/spiderman1/spider1_runtime.cpp)
    target_sources(spider1_catalog_test PRIVATE
      ${SPIDER1_FRAME_SOURCES} ${SPIDER1_WIDESCREEN_SOURCES} titles/spiderman1/spider1_catalog.cpp)
    target_include_directories(spider1_catalog_test PRIVATE ${PSXPORT_DIR}/tests)
    add_test(NAME spider1_catalog COMMAND spider1_catalog_test WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    add_test(NAME launcher_help
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_help_contract.py
              --executable $<TARGET_FILE:spiderman_port>)
  endif()
  add_test(NAME cpp_policy
    COMMAND ${Python3_EXECUTABLE} ${PSXPORT_DIR}/tools/check_cpp_style.py
            --root ${CMAKE_SOURCE_DIR} --compile-commands ${CMAKE_BINARY_DIR}
            # Shrink-only caps; lower them whenever the file shrinks.
            --cap titles/spiderman1/spider1_frame_driver.cpp=441
            --cap titles/spiderman1/spider1_mode_driver.cpp=521)
endif()
