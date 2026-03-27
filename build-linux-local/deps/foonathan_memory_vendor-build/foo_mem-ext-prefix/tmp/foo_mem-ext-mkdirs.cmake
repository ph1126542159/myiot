# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/src/foo_mem-ext")
  file(MAKE_DIRECTORY "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/src/foo_mem-ext")
endif()
file(MAKE_DIRECTORY
  "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/src/foo_mem-ext-build"
  "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix"
  "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/tmp"
  "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/src/foo_mem-ext-stamp"
  "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/src"
  "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/src/foo_mem-ext-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/src/foo_mem-ext-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor-build/foo_mem-ext-prefix/src/foo_mem-ext-stamp${cfgdir}") # cfgdir has leading slash
endif()
