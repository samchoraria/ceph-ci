function (set_library_properties_for_external_project _target _lib)
  set(_libfullname "${CMAKE_SHARED_LIBRARY_PREFIX}${_lib}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(_libpath "${CMAKE_BINARY_DIR}/external/lib")
  set(_includepath "${CMAKE_BINARY_DIR}/external/include")
  message(STATUS "Configuring ${_target} with ${_libpath}")

  # Manually create the directory, it will be created as part of the build,
  # but this runs in the configuration phase, and CMake generates an error if
  # we add an include directory that does not exist yet.
  file(MAKE_DIRECTORY "${_includepath}")
  set_target_properties(${_target} PROPERTIES
    INTERFACE_LINK_LIBRARIES "${_libpath}"
    IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
    IMPORTED_LOCATION "${_libpath}"
    INTERFACE_INCLUDE_DIRECTORIES "${_includepath}"
    INSTALL_RPATH "${CMAKE_BINARY_DIR}/external/lib")
endfunction ()
