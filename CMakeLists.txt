cmake_minimum_required(VERSION 3.20)

project(MyIoT LANGUAGES C CXX)

include(GNUInstallDirs)

if(CMAKE_CONFIGURATION_TYPES)
    set(MYIOT_MULTI_CONFIG_TYPES
        "Debug;Release;RelWithDebInfo"
        CACHE STRING "Configurations generated for multi-config generators" FORCE)
    set(CMAKE_CONFIGURATION_TYPES
        "${MYIOT_MULTI_CONFIG_TYPES}"
        CACHE STRING "Available build configurations" FORCE)
    message(STATUS
        "Using multi-config build types: ${CMAKE_CONFIGURATION_TYPES} "
        "(RelWithDebInfo is the release-with-debug-info configuration.)"
    )
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    add_compile_options(/FS /utf-8)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "Use the DLL runtime for protobuf on MSVC" FORCE)
endif()

add_compile_definitions(POCO_NO_AUTOMATIC_LIBS)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(MYIOT_PLATFORM_OSNAME "Windows_NT")
else()
    set(MYIOT_PLATFORM_OSNAME "${CMAKE_SYSTEM_NAME}")
endif()

if(CMAKE_VS_PLATFORM_NAME STREQUAL "x64")
    set(MYIOT_PLATFORM_OSARCH "AMD64")
elseif(CMAKE_VS_PLATFORM_NAME STREQUAL "Win32")
    set(MYIOT_PLATFORM_OSARCH "x86")
elseif(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
    set(MYIOT_PLATFORM_OSARCH "ARM64")
elseif(CMAKE_SYSTEM_PROCESSOR)
    set(MYIOT_PLATFORM_OSARCH "${CMAKE_SYSTEM_PROCESSOR}")
else()
    set(MYIOT_PLATFORM_OSARCH "unknown")
endif()

message(STATUS "Target OSP platform: ${MYIOT_PLATFORM_OSNAME}/${MYIOT_PLATFORM_OSARCH}")

add_compile_definitions(
    POCO_TARGET_OSNAME="${MYIOT_PLATFORM_OSNAME}"
    POCO_TARGET_OSARCH="${MYIOT_PLATFORM_OSARCH}"
)

set(MYIOT_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(MYIOT_LIBRARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(MYIOT_ARCHIVE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MYIOT_RUNTIME_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MYIOT_LIBRARY_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MYIOT_ARCHIVE_OUTPUT_DIR}")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${MYIOT_RUNTIME_OUTPUT_DIR}")

if(CMAKE_CONFIGURATION_TYPES)
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${config}" config_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_RUNTIME_OUTPUT_DIR}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_LIBRARY_OUTPUT_DIR}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_ARCHIVE_OUTPUT_DIR}")
        set(CMAKE_PDB_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_RUNTIME_OUTPUT_DIR}")
    endforeach()
endif()

set(CMAKE_INSTALL_PREFIX
    "${CMAKE_BINARY_DIR}/install"
    CACHE PATH "Install path prefix" FORCE)

set(MYIOT_DEPS_DIR
    "${CMAKE_BINARY_DIR}/deps"
    CACHE PATH "Directory used to download and build third-party dependencies")

option(MYIOT_ENABLE_FASTDDS_BOOTSTRAP "Download and build Fast-DDS when it is not available locally" ON)
option(MYIOT_ENABLE_OPENTELEMETRY "Enable OpenTelemetry C++ integration targets" OFF)
option(MYIOT_ENABLE_OPENTELEMETRY_BOOTSTRAP "Download and build official OpenTelemetry C++ when it is not available locally" ON)
option(MYIOT_BOOTSTRAP_SHOW_PROGRESS "Stream third-party download/build/install progress live in the configure output" ON)

set(MYIOT_FASTDDS_VERSION "v3.3.0" CACHE STRING "Fast-DDS version to bootstrap when missing")
set(MYIOT_FASTCDR_VERSION "v2.3.0" CACHE STRING "Fast-CDR version paired with the selected Fast-DDS version")
set(MYIOT_FOONATHAN_MEMORY_VENDOR_VERSION "v1.3.1" CACHE STRING "foonathan_memory_vendor version paired with the selected Fast-DDS version")
set(MYIOT_OPENTELEMETRY_VERSION "v1.26.0" CACHE STRING "Official OpenTelemetry C++ tag to bootstrap when missing")
set(MYIOT_OPENTELEMETRY_PACKAGE_VERSION "1.26.0" CACHE STRING "Official OpenTelemetry C++ package version paired with the selected source tag")
set(MYIOT_OPENTELEMETRY_SOURCE_DIR
    "${MYIOT_DEPS_DIR}/opentelemetry-cpp"
    CACHE PATH "Directory used to download the official OpenTelemetry C++ source tree")
option(MYIOT_OPENTELEMETRY_ENABLE_OSTREAM_EXPORTER "Enable OpenTelemetry ostream exporters" ON)
option(MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS "Enable OpenTelemetry resource detectors" ON)
option(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER "Enable OpenTelemetry OTLP HTTP exporters" ON)

option(MYIOT_ENABLE_MACCHINA_PLATFORM "Build selected macchina.io platform modules" ON)
option(MYIOT_ENABLE_WEBUI "Build Web UI bundles" ON)
option(MYIOT_ENABLE_JS_MODULES "Build JS/V8-related platform modules" OFF)
option(MYIOT_ENABLE_POCO_BOOTSTRAP "Download and build official Poco when it is not available locally" ON)
option(MYIOT_ENABLE_HTTPS_BUNDLES "Build HTTPS-related OSP bundles" OFF)
option(MYIOT_ENABLE_DATA_MODULES "Build Poco Data/DataSQLite/ActiveRecord modules and OSP data bundles" OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(POCO_UNBUNDLED OFF CACHE BOOL "Use external third-party dependencies" FORCE)
set(POCO_DISABLE_INTERNAL_OPENSSL ON CACHE BOOL "Use externally provided OpenSSL" FORCE)
set(DISABLE_CPP11 OFF CACHE BOOL "Disable C++11 support" FORCE)
set(DISABLE_CPP14 OFF CACHE BOOL "Disable C++14 support" FORCE)
set(MYSQL_FOUND OFF CACHE BOOL "Disable MySQL support" FORCE)
set(POSTGRESQL_FOUND OFF CACHE BOOL "Disable PostgreSQL support" FORCE)
set(ODBC_FOUND OFF CACHE BOOL "Disable ODBC support" FORCE)
set(ENABLE_DATA_MYSQL OFF CACHE BOOL "Disable Poco Data MySQL support" FORCE)
set(ENABLE_DATA_POSTGRESQL OFF CACHE BOOL "Disable Poco Data PostgreSQL support" FORCE)
set(ENABLE_DATA_ODBC OFF CACHE BOOL "Disable Poco Data ODBC support" FORCE)
set(SHARED_LIBRARY_VERSION 1 CACHE STRING "Compatibility version for imported POCO modules")

set(MYIOT_POCO_VERSION "poco-1.11.6-release" CACHE STRING "Official Poco version to bootstrap when missing")
set(MYIOT_POCO_PACKAGE_VERSION "1.11.6" CACHE STRING "Official Poco package version paired with the selected source tag")
set(MYIOT_POCO_SOURCE_DIR
    "${MYIOT_DEPS_DIR}/Poco"
    CACHE PATH "Directory used to download the official Poco source tree")
set(MYIOT_POCO_COMPONENTS
    Foundation
    XML
    JSON
    Util
    Net
    Crypto
    NetSSL
    Zip
    CppParser
    Redis
    JWT
)

if(MYIOT_ENABLE_DATA_MODULES)
    list(APPEND MYIOT_POCO_COMPONENTS
        Data
        DataSQLite
        ActiveRecord
    )
endif()

set(MACCHINA_PLATFORM_DIR
    "${CMAKE_SOURCE_DIR}/platform"
    CACHE PATH "Path to the vendored macchina.io platform source tree" FORCE)

include_directories("${CMAKE_SOURCE_DIR}/platform/OpenTelemetry/include")
add_compile_definitions(
    MYIOT_POCO_OPENTELEMETRY_ENABLED=$<IF:$<BOOL:${MYIOT_ENABLE_OPENTELEMETRY}>,1,0>
)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}")

include(MyIoTThirdPartyCommon)
include(MyIoTFastDDS)
include(MyIoTOpenSSL)
include(MyIoTOpenTelemetry)
include(MyIoTPoco)

set(MYIOT_OSP_BUNDLE_CREATOR_COMMAND "$<TARGET_FILE:OSPBundleCreator>")
if(CMAKE_CROSSCOMPILING)
    if(NOT DEFINED MYIOT_QEMU_ARM OR NOT EXISTS "${MYIOT_QEMU_ARM}")
        message(FATAL_ERROR
            "Cross-compiling this project requires a host-side qemu-arm executable. "
            "Pass MYIOT_QEMU_ARM or use cmake/toolchains/petalinux-jndm123.cmake.")
    endif()

    set(MYIOT_OSP_BUNDLE_CREATOR_COMMAND
        "${MYIOT_QEMU_ARM}"
        "-L"
        "${CMAKE_SYSROOT}"
        "$<TARGET_FILE:OSPBundleCreator>"
    )
endif()

if(MYIOT_ENABLE_MACCHINA_PLATFORM)
    if(EXISTS "${MACCHINA_PLATFORM_DIR}/CodeGeneration"
       AND EXISTS "${MACCHINA_PLATFORM_DIR}/Geo"
       AND EXISTS "${MACCHINA_PLATFORM_DIR}/Serial"
       AND EXISTS "${MACCHINA_PLATFORM_DIR}/WebTunnel"
       AND EXISTS "${MACCHINA_PLATFORM_DIR}/OSP")
        message(STATUS "Using vendored macchina.io platform extension sources at: ${MACCHINA_PLATFORM_DIR}")
    else()
        message(FATAL_ERROR "macchina.io platform sources not found: ${MACCHINA_PLATFORM_DIR}")
    endif()

    if(CMAKE_CROSSCOMPILING)
        message(STATUS "Cross-compiling with sysroot: ${CMAKE_SYSROOT}")
        message(STATUS "Using qemu-arm for host-side bundle packaging: ${MYIOT_QEMU_ARM}")
    endif()

    include("${CMAKE_SOURCE_DIR}/cmake/PocoCompat.cmake")

    add_subdirectory("${MACCHINA_PLATFORM_DIR}/CodeGeneration" "${CMAKE_BINARY_DIR}/platform/CodeGeneration")
    add_subdirectory("${MACCHINA_PLATFORM_DIR}/Geo" "${CMAKE_BINARY_DIR}/platform/Geo")
    add_subdirectory("${MACCHINA_PLATFORM_DIR}/Serial" "${CMAKE_BINARY_DIR}/platform/Serial")
    add_subdirectory("${MACCHINA_PLATFORM_DIR}/WebTunnel" "${CMAKE_BINARY_DIR}/platform/WebTunnel")
    add_subdirectory("${MACCHINA_PLATFORM_DIR}/OSP" "${CMAKE_BINARY_DIR}/platform/OSP")
    if(MYIOT_ENABLE_OPENTELEMETRY)
        add_subdirectory("${CMAKE_SOURCE_DIR}/platform/OpenTelemetry" "${CMAKE_BINARY_DIR}/platform/OpenTelemetry")
        if(TARGET OSPWeb AND TARGET Poco::OpenTelemetry)
            target_link_libraries(OSPWeb PUBLIC Poco::OpenTelemetry)
        endif()
        if(TARGET OSP_Web_Bundle AND TARGET Poco::OpenTelemetry)
            target_link_libraries(OSP_Web_Bundle PRIVATE Poco::OpenTelemetry)
        endif()
    endif()

    set(MYIOT_PLATFORM_TARGETS
        CodeGeneration
        Geo
        Serial
        WebTunnel
        OSP
        OSPWeb
        OSPWebEvent
        OSPUnit
        OSP_Core_Bundle
        OSP_BundleAdmin_Bundle
        OSPBundleCreator
        OSPCodeCacheUtility
        OSP_Net_Bundle
        OSP_SimpleAuth_Bundle
        OSPStripBundle
        OSP_Web_Bundle
        OSP_WebEvent_Bundle
        OSP_WebServer_Bundle
    )

    if(TARGET OpenTelemetry)
        list(APPEND MYIOT_PLATFORM_TARGETS
            OpenTelemetry
        )
    endif()

    if(MYIOT_ENABLE_DATA_MODULES)
        list(APPEND MYIOT_PLATFORM_TARGETS
            OSP_ActiveRecord_Bundle
            OSP_Data_Bundle
            OSP_Data_SQLite_Bundle
        )
    endif()

    if(MYIOT_ENABLE_HTTPS_BUNDLES)
        list(APPEND MYIOT_PLATFORM_TARGETS
            OSP_Crypto_Bundle
            OSP_NetSSL_Bundle
            OSP_SecureWebServer_Bundle
        )
    endif()

    add_library(myiot_poco_platform INTERFACE)
    target_link_libraries(myiot_poco_platform
        INTERFACE
            Poco::Foundation
            Poco::XML
            Poco::JSON
            Poco::Util
            Poco::Net
            Poco::Crypto
            Poco::NetSSL
            Poco::Zip
            Poco::CppParser
            Poco::Redis
            Poco::JWT
            Poco::CppUnit
            Poco::CodeGeneration
            Poco::Geo
            Poco::Serial
            Poco::WebTunnel
            Poco::OSP
            Poco::OSPWeb
            Poco::OSPWebEvent
            Poco::OSPUnit
    )

    if(TARGET Poco::OpenTelemetry)
        target_link_libraries(myiot_poco_platform
            INTERFACE
                Poco::OpenTelemetry
        )
    endif()

    if(MYIOT_ENABLE_DATA_MODULES)
        target_link_libraries(myiot_poco_platform
            INTERFACE
                Poco::Data
                Poco::DataSQLite
                Poco::ActiveRecord
        )
    endif()

    install(TARGETS ${MYIOT_PLATFORM_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(DIRECTORY "${CMAKE_BINARY_DIR}/bundles/"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/bundles"
        OPTIONAL
        FILES_MATCHING PATTERN "*.bndl"
    )
endif()

if(MYIOT_ENABLE_WEBUI)
    add_subdirectory(webUI)
endif()
add_subdirectory(services)
add_subdirectory(server)
