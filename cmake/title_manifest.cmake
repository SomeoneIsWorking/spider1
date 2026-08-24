function(spider_read_title TITLE_ID PREFIX)
  # tools/title_catalog.py is the one schema/identity validator. CMake consumes the same manifest
  # values needed to bind one target; it does not maintain a second serial/hash grammar.
  set(_manifest "${CMAKE_SOURCE_DIR}/titles/${TITLE_ID}/title.json")
  if(NOT EXISTS "${_manifest}")
    message(FATAL_ERROR "missing title manifest: ${_manifest}")
  endif()
  file(READ "${_manifest}" _json)
  foreach(_field
      serial
      discEnv
      target
      guestExecutable
      generatedDirectory
      generatedMain
      fileSize
      executableSha256)
    string(JSON _value ERROR_VARIABLE _error GET "${_json}" "${_field}")
    if(_error)
      message(FATAL_ERROR "${_manifest}: missing or invalid ${_field}: ${_error}")
    endif()
    string(TOUPPER "${_field}" _upper)
    set("${PREFIX}_${_upper}" "${_value}" PARENT_SCOPE)
  endforeach()
endfunction()

function(spider_configure_target TARGET TITLE_PREFIX)
  add_dependencies(${TARGET} gen_gpu_shaders)
  set_target_properties(${TARGET} PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON
    ENABLE_EXPORTS ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/scratch/bin)
  target_include_directories(${TARGET} PRIVATE game game/core game/render)
  target_compile_definitions(${TARGET} PRIVATE
    SPIDER_TITLE_SERIAL="${${TITLE_PREFIX}_SERIAL}"
    SPIDER_TITLE_DISC_ENV="${${TITLE_PREFIX}_DISCENV}"
    SPIDER_TITLE_GUEST_EXE="${${TITLE_PREFIX}_GUESTEXECUTABLE}"
    SPIDER_TITLE_EXECUTABLE_SIZE=${${TITLE_PREFIX}_FILESIZE}
    SPIDER_TITLE_EXECUTABLE_SHA256="${${TITLE_PREFIX}_EXECUTABLESHA256}")
  target_compile_options(${TARGET} PRIVATE -O2 -g
    ${SDL3_CFLAGS_OTHER} ${FREETYPE_CFLAGS_OTHER})
  target_link_libraries(${TARGET} PRIVATE psxport OpenSSL::Crypto)
endfunction()
