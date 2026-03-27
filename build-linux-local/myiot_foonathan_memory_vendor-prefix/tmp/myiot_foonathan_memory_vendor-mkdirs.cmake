# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor")
  file(MAKE_DIRECTORY "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor")
endif()
file(MAKE_DIRECTORY
  "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build"
  "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix"
  "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/tmp"
  "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp"
  "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src"
  "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp${cfgdir}") # cfgdir has leading slash
endif()
