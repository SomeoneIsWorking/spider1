function(spider_read_title TITLE_ID PREFIX)
  # tools/title_catalog.py owns manifest validation; CMake only consumes the values.
  set(_manifest "${CMAKE_SOURCE_DIR}/titles/${TITLE_ID}/title.json")
  if(NOT EXISTS "${_manifest}")
    message(FATAL_ERROR "missing title manifest: ${_manifest}")
  endif()
  file(READ "${_manifest}" _json)
  foreach(_field
      id
      label
      serial
      discEnv
      target
      guestExecutable
      fileSize
      executableSha256)
    string(JSON _value ERROR_VARIABLE _error GET "${_json}" "${_field}")
    if(_error)
      message(FATAL_ERROR "${_manifest}: missing or invalid ${_field}: ${_error}")
    endif()
    string(TOUPPER "${_field}" _upper)
    set("${PREFIX}_${_upper}" "${_value}" PARENT_SCOPE)
  endforeach()
  # The PS-X EXE header words are optional until a title joins the host catalog.
  foreach(_field entry gp textAddress textSize stackAddress stackOffset)
    string(JSON _value ERROR_VARIABLE _error GET "${_json}" header ${_field})
    if(NOT _error)
      string(TOUPPER "${_field}" _upper)
      set("${PREFIX}_HEADER_${_upper}" "${_value}" PARENT_SCOPE)
    endif()
  endforeach()
endfunction()
