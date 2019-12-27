# Try to find shared library on the system
find_package(OpenCASCADE QUIET)
if(OpenCASCADE_FOUND)
  message(STATUS "Using system OpenCASCADE")

  include_directories(${OpenCASCADE_INCLUDE_DIR})
  message("OCC-Include-Dir: ${OpenCASCADE_INCLUDE_DIR}")
  link_directories(${OpenCASCADE_LIBRARY_DIR})
  message("OCC-Lib-Dir: ${OpenCASCADE_LIBRARY_DIR}")
  message("OCC-Libs: ${OpenCASCADE_LIBRARIES}")

  # Stop here, we're done
  return()
endif()

message(FATAL_ERROR "Did not find OpenCASCADE library!")
