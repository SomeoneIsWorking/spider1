spider_read_title(spiderman2 SPIDER2)

set(_gen_dir "${CMAKE_SOURCE_DIR}/${SPIDER2_GENERATEDDIRECTORY}")
set(_manifest "${_gen_dir}/rec_sources.cmake")
if(NOT EXISTS "${_manifest}")
  message(FATAL_ERROR
    "Enter Electro substrate is absent: run `uv run --frozen python tools/ensure_recomp.py --title spiderman2 <disc.chd>`")
endif()
include("${_manifest}")
list(TRANSFORM GEN_REC_SRCS PREPEND "${SPIDER2_GENERATEDDIRECTORY}/")
set_source_files_properties(${GEN_REC_SRCS}
  PROPERTIES LANGUAGE CXX
  COMPILE_OPTIONS "-w;-O1;-foptimize-sibling-calls;-fno-strict-aliasing;-fwrapv")

add_executable(${SPIDER2_TARGET}
  game/core/spider_port.cpp
  game/core/executable_identity.cpp
  game/core/spider_runtime.cpp
  titles/spiderman2/main.cpp
  titles/spiderman2/enter_electro_runtime.cpp
  titles/spiderman2/recomp_register.cpp
  ${GEN_REC_SRCS})
spider_configure_target(${SPIDER2_TARGET} SPIDER2)
target_include_directories(${SPIDER2_TARGET} PRIVATE
  titles/spiderman2
  "${_gen_dir}")
