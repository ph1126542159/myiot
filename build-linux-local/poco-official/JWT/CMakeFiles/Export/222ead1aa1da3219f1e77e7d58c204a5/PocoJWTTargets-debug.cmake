#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Poco::JWT" for configuration "Debug"
set_property(TARGET Poco::JWT APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Poco::JWT PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libPocoJWTd.a"
  )

list(APPEND _cmake_import_check_targets Poco::JWT )
list(APPEND _cmake_import_check_files_for_Poco::JWT "${_IMPORT_PREFIX}/lib/libPocoJWTd.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
