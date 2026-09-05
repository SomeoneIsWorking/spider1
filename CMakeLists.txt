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
    target_sources(spiderman_port PRIVATE titles/spiderman1/spider1_movie_execution.cpp)
  elseif(SPIDER_TITLE STREQUAL "spiderman2")
    spider_add_runtime_target(
      enter_electro_port spiderman2 SPIDER2
      titles/spiderman2/main.cpp titles/spiderman2/enter_electro_runtime.cpp)
  else()
    message(FATAL_ERROR "unknown SPIDER_TITLES entry '${SPIDER_TITLE}'")
  endif()
endforeach()

# Keep the preserved Spider-Man native owners compile-checked while the break-first runtime brings
# them back through image-aware dispatch one boundary at a time. They are intentionally not linked
# into the player until Spider1Runtime owns their lifecycle and has runtime evidence for the route.
if(TARGET spiderman_port)
  add_library(spider1_native_owners_compile OBJECT
    game/core/cd_stream.cpp
    titles/spiderman1/spider1_frame_driver.cpp
    titles/spiderman1/spider1_mode_driver.cpp
    titles/spiderman1/spider1_widescreen.cpp)
  target_include_directories(spider1_native_owners_compile PRIVATE
    game/core
    game/render
    titles/spiderman1)
  target_link_libraries(spider1_native_owners_compile PRIVATE psxport)
endif()

include(CTest)
if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
  add_test(NAME source_policy
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/source_policy.py)
  add_test(NAME source_policy_selftest
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/source_policy.py --selftest)
  if(TARGET spiderman_port)
    add_test(NAME launcher_help
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_help_contract.py
              --executable $<TARGET_FILE:spiderman_port>)
  endif()
  add_test(NAME psxport_pin
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/psxport_sync.py
            --check --build-dir ${CMAKE_BINARY_DIR})
  add_test(NAME cpp_policy
    COMMAND ${Python3_EXECUTABLE} ${PSXPORT_DIR}/tools/check_cpp_style.py
            --root ${CMAKE_SOURCE_DIR} --compile-commands ${CMAKE_BINARY_DIR})
endif()
