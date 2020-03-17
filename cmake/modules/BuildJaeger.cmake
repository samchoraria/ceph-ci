# This module builds Jaeger after it's dependencies are installed and discovered
# OpenTracing: is built using cmake/modules/BuildOpenTracing.cmake
# Thrift: build using cmake/modules/Buildthrift.cmake
# yaml-cpp, nlhomann-json: are installed locally and then discovered using
# Find<package>.cmake
# Boost Libraries used for building thrift are build and provided by
# cmake/modules/BuildBoost.cmake

function(build_jaeger)
  set(Jaeger_DOWNLOAD_DIR "${CMAKE_SOURCE_DIR}/src/jaegertracing")
  set(Jaeger_SOURCE_DIR "${CMAKE_SOURCE_DIR}/src/jaegertracing/jaeger-client-cpp")
  set(Jaeger_ROOT_DIR "${CMAKE_BINARY_DIR}/external")
  set(Jaeger_BINARY_DIR "${Jaeger_ROOT_DIR}/Jaeger")
  list(APPEND CMAKE_FIND_ROOT_PATH "${CMAKE_BINARY_DIR}/external")

  set(Jaeger_CMAKE_ARGS -DCMAKE_POSITION_INDEPENDENT_CODE=ON
			-DBUILD_SHARED_LIBS=ON
			-DHUNTER_ENABLED=OFF
			-DBUILD_TESTING=OFF
			-DBUILD_EXAMPLES=OFF
			-DCMAKE_FIND_ROOT_PATH=${CMAKE_BINARY_DIR}/external
			#-DCMAKE_PREFIX_PATH={CMAKE_BINARY_DIR}/external
			-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external
			-DCMAKE_FIND_ROOT_PATH=${CMAKE_BINARY_DIR}/external
			-DCMAKE_INSTALL_LIBDIR=${CMAKE_BINARY_DIR}/external/lib)

  include(BuildOpenTracing)
  build_opentracing()
  include(Buildthrift)
  build_thrift()

  if(CMAKE_MAKE_PROGRAM MATCHES "make")
    # try to inherit command line arguments passed by parent "make" job
    set(make_cmd $(MAKE))
  else()
    set(make_cmd ${CMAKE_COMMAND} --build <BINARY_DIR> --target Jaeger)
  endif()

  include(ExternalProject)
  ExternalProject_Add(Jaeger
    GIT_REPOSITORY https://github.com/ideepika/jaeger-client-cpp.git
    GIT_TAG "fixes-issue-162"
    UPDATE_COMMAND ""
    INSTALL_DIR "${CMAKE_BINARY_DIR}/external"
    DOWNLOAD_DIR ${Jaeger_DOWNLOAD_DIR}
    SOURCE_DIR ${Jaeger_SOURCE_DIR}
    PREFIX ${Jaeger_ROOT_DIR}
    CMAKE_ARGS ${Jaeger_CMAKE_ARGS}
    BINARY_DIR ${Jaeger_BINARY_DIR}
    BUILD_COMMAND ${make_cmd}
    INSTALL_COMMAND make install
    DEPENDS OpenTracing thrift yaml-cpp::yaml-cpp
    )
endfunction()
