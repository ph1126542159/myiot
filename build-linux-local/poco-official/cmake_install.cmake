# Install script for directory: /home/ph/work/myiot/build-linux-local/deps/Poco

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/home/ph/work/myiot/build-linux-local/install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Devel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/Poco" TYPE FILE FILES
    "/home/ph/work/myiot/build-linux-local/poco-official/Poco/PocoConfig.cmake"
    "/home/ph/work/myiot/build-linux-local/poco-official/Poco/PocoConfigVersion.cmake"
    )
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/ph/work/myiot/build-linux-local/poco-official/CppUnit/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/Foundation/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/XML/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/JSON/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/Util/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/Net/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/Redis/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/JWT/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/NetSSL_OpenSSL/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/Crypto/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/Zip/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/CppParser/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/PageCompiler/cmake_install.cmake")
  include("/home/ph/work/myiot/build-linux-local/poco-official/PageCompiler/File2Page/cmake_install.cmake")

endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/home/ph/work/myiot/build-linux-local/poco-official/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
