# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

if(EXISTS "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/myiot_foonathan_memory_vendor-gitclone-lastrun.txt" AND EXISTS "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/myiot_foonathan_memory_vendor-gitinfo.txt" AND
  "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/myiot_foonathan_memory_vendor-gitclone-lastrun.txt" IS_NEWER_THAN "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/myiot_foonathan_memory_vendor-gitinfo.txt")
  message(VERBOSE
    "Avoiding repeated git clone, stamp file is up to date: "
    "'/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/myiot_foonathan_memory_vendor-gitclone-lastrun.txt'"
  )
  return()
endif()

# Even at VERBOSE level, we don't want to see the commands executed, but
# enabling them to be shown for DEBUG may be useful to help diagnose problems.
cmake_language(GET_MESSAGE_LOG_LEVEL active_log_level)
if(active_log_level MATCHES "DEBUG|TRACE")
  set(maybe_show_command COMMAND_ECHO STDOUT)
else()
  set(maybe_show_command "")
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor"
  RESULT_VARIABLE error_code
  ${maybe_show_command}
)
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: '/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "/usr/bin/git"
            clone --no-checkout --depth 1 --no-single-branch --config "advice.detachedHead=false" "https://github.com/eProsima/foonathan_memory_vendor.git" "foonathan_memory_vendor"
    WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/deps"
    RESULT_VARIABLE error_code
    ${maybe_show_command}
  )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(NOTICE "Had to git clone more than once: ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://github.com/eProsima/foonathan_memory_vendor.git'")
endif()

execute_process(
  COMMAND "/usr/bin/git"
          checkout "v1.3.1" --
  WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor"
  RESULT_VARIABLE error_code
  ${maybe_show_command}
)
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'v1.3.1'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "/usr/bin/git" 
            submodule update --recursive --init 
    WORKING_DIRECTORY "/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor"
    RESULT_VARIABLE error_code
    ${maybe_show_command}
  )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: '/home/ph/work/myiot/build-linux-local/deps/foonathan_memory_vendor'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/myiot_foonathan_memory_vendor-gitinfo.txt" "/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/myiot_foonathan_memory_vendor-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  ${maybe_show_command}
)
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: '/home/ph/work/myiot/build-linux-local/myiot_foonathan_memory_vendor-prefix/src/myiot_foonathan_memory_vendor-stamp/myiot_foonathan_memory_vendor-gitclone-lastrun.txt'")
endif()
