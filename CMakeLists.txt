cmake_minimum_required(VERSION 3.20)

project(MyIoT LANGUAGES C CXX)

include(GNUInstallDirs)

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
set(MYIOT_COMPILE_PDB_OUTPUT_DIR "${CMAKE_BINARY_DIR}/obj")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${MYIOT_RUNTIME_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${MYIOT_LIBRARY_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${MYIOT_ARCHIVE_OUTPUT_DIR}")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${MYIOT_RUNTIME_OUTPUT_DIR}")
set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY "${MYIOT_COMPILE_PDB_OUTPUT_DIR}")

if(CMAKE_CONFIGURATION_TYPES)
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${config}" config_upper)
        set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_RUNTIME_OUTPUT_DIR}")
        set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_LIBRARY_OUTPUT_DIR}")
        set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_ARCHIVE_OUTPUT_DIR}")
        set(CMAKE_PDB_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_RUNTIME_OUTPUT_DIR}")
        set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY_${config_upper} "${MYIOT_COMPILE_PDB_OUTPUT_DIR}")
    endforeach()
endif()

set(CMAKE_INSTALL_PREFIX
    "${CMAKE_BINARY_DIR}/install"
    CACHE PATH "Install path prefix" FORCE)

set(MYIOT_DEPS_DIR
    "${CMAKE_BINARY_DIR}/deps"
    CACHE PATH "Directory used to download and build third-party dependencies")

option(MYIOT_ENABLE_FASTDDS_BOOTSTRAP "Download and build Fast-DDS when it is not available locally" OFF)
option(MYIOT_ENABLE_OPENTELEMETRY "Enable OpenTelemetry C++ integration targets" OFF)
option(MYIOT_ENABLE_OPENTELEMETRY_BOOTSTRAP "Download and build official OpenTelemetry C++ when it is not available locally" OFF)

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

list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}")

if(MYIOT_ENABLE_FASTDDS_BOOTSTRAP)
    find_package(fastdds CONFIG QUIET)

    if(fastdds_FOUND)
        message(STATUS "Found Fast-DDS package: ${fastdds_DIR}")
    else()
        include(ExternalProject)
        find_package(Git REQUIRED)

        file(MAKE_DIRECTORY "${MYIOT_DEPS_DIR}")

        set(MYIOT_FASTDDS_PREFIX "${CMAKE_INSTALL_PREFIX}")
        set(_myiot_fastdds_external_cmake_args
            -DCMAKE_INSTALL_PREFIX:PATH=${MYIOT_FASTDDS_PREFIX}
            -DCMAKE_PREFIX_PATH:PATH=${MYIOT_FASTDDS_PREFIX}
            -DBUILD_TESTING:BOOL=OFF
        )

        if(CMAKE_C_COMPILER)
            list(APPEND _myiot_fastdds_external_cmake_args
                -DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}
            )
        endif()
        if(CMAKE_C_COMPILER_ARG1)
            list(APPEND _myiot_fastdds_external_cmake_args
                -DCMAKE_C_COMPILER_ARG1:STRING=${CMAKE_C_COMPILER_ARG1}
            )
        endif()
        if(CMAKE_CXX_COMPILER)
            list(APPEND _myiot_fastdds_external_cmake_args
                -DCMAKE_CXX_COMPILER:FILEPATH=${CMAKE_CXX_COMPILER}
            )
        endif()
        if(CMAKE_CXX_COMPILER_ARG1)
            list(APPEND _myiot_fastdds_external_cmake_args
                -DCMAKE_CXX_COMPILER_ARG1:STRING=${CMAKE_CXX_COMPILER_ARG1}
            )
        endif()
        if(CMAKE_MAKE_PROGRAM)
            list(APPEND _myiot_fastdds_external_cmake_args
                -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
            )
        endif()

        if(NOT CMAKE_CONFIGURATION_TYPES AND CMAKE_BUILD_TYPE)
            list(APPEND _myiot_fastdds_external_cmake_args
                -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
            )
        endif()

        set(_myiot_fastdds_build_command
            ${CMAKE_COMMAND} --build <BINARY_DIR> --parallel
        )
        set(_myiot_fastdds_install_command
            ${CMAKE_COMMAND} --build <BINARY_DIR> --target install --parallel
        )

        if(CMAKE_CONFIGURATION_TYPES)
            set(_myiot_fastdds_build_command
                ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --parallel
            )
            set(_myiot_fastdds_install_command
                ${CMAKE_COMMAND} --build <BINARY_DIR> --config $<CONFIG> --target install --parallel
            )
        endif()

        set(_myiot_fastdds_project_cmake_args
            ${_myiot_fastdds_external_cmake_args}
            -DTHIRDPARTY:STRING=ON
            -DTHIRDPARTY_Asio:STRING=ON
            -DTHIRDPARTY_TinyXML2:STRING=ON
            -DTHIRDPARTY_UPDATE:BOOL=OFF
        )

        message(STATUS "Fast-DDS was not found. Sources will be cloned under: ${MYIOT_DEPS_DIR}")
        message(STATUS "Bootstrapped Fast-DDS artifacts will be installed to: ${MYIOT_FASTDDS_PREFIX}")

        ExternalProject_Add(myiot_foonathan_memory_vendor
            GIT_REPOSITORY https://github.com/eProsima/foonathan_memory_vendor.git
            GIT_TAG ${MYIOT_FOONATHAN_MEMORY_VENDOR_VERSION}
            GIT_SHALLOW TRUE
            UPDATE_DISCONNECTED TRUE
            SOURCE_DIR "${MYIOT_DEPS_DIR}/foonathan_memory_vendor"
            BINARY_DIR "${MYIOT_DEPS_DIR}/foonathan_memory_vendor-build"
            CMAKE_GENERATOR "${CMAKE_GENERATOR}"
            CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
            CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}"
            CMAKE_ARGS ${_myiot_fastdds_external_cmake_args}
            BUILD_COMMAND ${_myiot_fastdds_build_command}
            INSTALL_COMMAND ${_myiot_fastdds_install_command}
            TEST_COMMAND ""
            USES_TERMINAL_DOWNLOAD TRUE
            USES_TERMINAL_BUILD TRUE
            USES_TERMINAL_INSTALL TRUE
        )

        ExternalProject_Add(myiot_fastcdr
            GIT_REPOSITORY https://github.com/eProsima/Fast-CDR.git
            GIT_TAG ${MYIOT_FASTCDR_VERSION}
            GIT_SHALLOW TRUE
            UPDATE_DISCONNECTED TRUE
            DEPENDS myiot_foonathan_memory_vendor
            SOURCE_DIR "${MYIOT_DEPS_DIR}/Fast-CDR"
            BINARY_DIR "${MYIOT_DEPS_DIR}/Fast-CDR-build"
            CMAKE_GENERATOR "${CMAKE_GENERATOR}"
            CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
            CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}"
            CMAKE_ARGS ${_myiot_fastdds_external_cmake_args}
            BUILD_COMMAND ${_myiot_fastdds_build_command}
            INSTALL_COMMAND ${_myiot_fastdds_install_command}
            TEST_COMMAND ""
            USES_TERMINAL_DOWNLOAD TRUE
            USES_TERMINAL_BUILD TRUE
            USES_TERMINAL_INSTALL TRUE
        )

        ExternalProject_Add(myiot_fastdds_external
            GIT_REPOSITORY https://github.com/eProsima/Fast-DDS.git
            GIT_TAG ${MYIOT_FASTDDS_VERSION}
            GIT_SHALLOW TRUE
            UPDATE_DISCONNECTED TRUE
            DEPENDS myiot_foonathan_memory_vendor myiot_fastcdr
            SOURCE_DIR "${MYIOT_DEPS_DIR}/Fast-DDS"
            BINARY_DIR "${MYIOT_DEPS_DIR}/Fast-DDS-build"
            CMAKE_GENERATOR "${CMAKE_GENERATOR}"
            CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
            CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}"
            CMAKE_ARGS ${_myiot_fastdds_project_cmake_args}
            BUILD_COMMAND ${_myiot_fastdds_build_command}
            INSTALL_COMMAND ${_myiot_fastdds_install_command}
            TEST_COMMAND ""
            USES_TERMINAL_DOWNLOAD TRUE
            USES_TERMINAL_BUILD TRUE
            USES_TERMINAL_INSTALL TRUE
        )

        add_custom_target(myiot_fastdds_bootstrap ALL
            DEPENDS myiot_fastdds_external
        )
    endif()
endif()

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

find_package(OpenSSL REQUIRED)

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

function(myiot_normalize_imported_target_locations target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    get_target_property(_myiot_is_imported ${target_name} IMPORTED)
    if(NOT _myiot_is_imported)
        return()
    endif()

    get_target_property(_myiot_debug_location ${target_name} IMPORTED_LOCATION_DEBUG)
    get_target_property(_myiot_release_location ${target_name} IMPORTED_LOCATION_RELEASE)

    if(NOT _myiot_release_location)
        set(_myiot_release_location "${_myiot_debug_location}")
    endif()
    if(NOT _myiot_debug_location)
        set(_myiot_debug_location "${_myiot_release_location}")
    endif()

    if(_myiot_debug_location)
        set_property(TARGET ${target_name} APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(${target_name} PROPERTIES IMPORTED_LOCATION_DEBUG "${_myiot_debug_location}")
    endif()

    if(_myiot_release_location)
        set_property(TARGET ${target_name} APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target_name} PROPERTIES
            IMPORTED_LOCATION_RELEASE "${_myiot_release_location}"
            IMPORTED_LOCATION_RELWITHDEBINFO "${_myiot_release_location}"
            IMPORTED_LOCATION_MINSIZEREL "${_myiot_release_location}"
        )
    endif()
endfunction()

if(MYIOT_ENABLE_OPENTELEMETRY)
    set(_myiot_local_opentelemetry_config_dir "${CMAKE_INSTALL_PREFIX}/lib/cmake/opentelemetry-cpp")
    if(EXISTS "${_myiot_local_opentelemetry_config_dir}/opentelemetry-cpp-config.cmake"
       AND (
            NOT EXISTS "${_myiot_local_opentelemetry_config_dir}/component-definitions.cmake"
            OR NOT EXISTS "${_myiot_local_opentelemetry_config_dir}/thirdparty-dependency-definitions.cmake"
            OR NOT EXISTS "${_myiot_local_opentelemetry_config_dir}/find-package-support-functions.cmake"
       ))
        message(STATUS
            "Removing incomplete OpenTelemetry C++ package metadata from install prefix: "
            "${_myiot_local_opentelemetry_config_dir}"
        )
        file(REMOVE_RECURSE "${_myiot_local_opentelemetry_config_dir}")
    endif()

    if(EXISTS "${_myiot_local_opentelemetry_config_dir}/component-definitions.cmake")
        set(_myiot_required_local_opentelemetry_components)
        if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
            list(APPEND _myiot_required_local_opentelemetry_components exporters_otlp_http)
        endif()

        if(_myiot_required_local_opentelemetry_components)
            file(READ
                "${_myiot_local_opentelemetry_config_dir}/component-definitions.cmake"
                _myiot_local_opentelemetry_component_definitions
            )

            foreach(_myiot_required_component IN LISTS _myiot_required_local_opentelemetry_components)
                string(FIND
                    "${_myiot_local_opentelemetry_component_definitions}"
                    "${_myiot_required_component}"
                    _myiot_required_component_index
                )
                if(_myiot_required_component_index EQUAL -1)
                    message(STATUS
                        "Installed OpenTelemetry package is missing requested component "
                        "${_myiot_required_component}. Removing local package metadata so it can be rebuilt."
                    )
                    file(REMOVE_RECURSE "${_myiot_local_opentelemetry_config_dir}")
                    break()
                endif()
            endforeach()
        endif()
    endif()

    set(_myiot_opentelemetry_core_components
        api
        sdk
    )
    set(_myiot_opentelemetry_optional_components)
    set(_myiot_opentelemetry_imported_targets
        opentelemetry-cpp::api
        opentelemetry-cpp::sdk
        opentelemetry-cpp::version
        opentelemetry-cpp::common
        opentelemetry-cpp::resources
        opentelemetry-cpp::trace
        opentelemetry-cpp::metrics
        opentelemetry-cpp::logs
    )

    if(MYIOT_OPENTELEMETRY_ENABLE_OSTREAM_EXPORTER)
        list(APPEND _myiot_opentelemetry_optional_components exporters_ostream)
        list(APPEND _myiot_opentelemetry_imported_targets
            opentelemetry-cpp::ostream_span_exporter
            opentelemetry-cpp::ostream_metrics_exporter
            opentelemetry-cpp::ostream_log_record_exporter
        )
    endif()

    if(MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS)
        list(APPEND _myiot_opentelemetry_optional_components resource_detectors)
        list(APPEND _myiot_opentelemetry_imported_targets
            opentelemetry-cpp::resource_detectors
        )
    endif()

    if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
        list(APPEND _myiot_opentelemetry_optional_components exporters_otlp_http)
        list(APPEND _myiot_opentelemetry_imported_targets
            opentelemetry-cpp::otlp_http_client
            opentelemetry-cpp::otlp_http_exporter
            opentelemetry-cpp::otlp_http_log_record_exporter
            opentelemetry-cpp::otlp_http_metric_exporter
        )
    endif()

    find_package(opentelemetry-cpp CONFIG QUIET COMPONENTS ${_myiot_opentelemetry_core_components})

    if(TARGET opentelemetry-cpp::api AND TARGET opentelemetry-cpp::sdk)
        message(STATUS "Found OpenTelemetry C++ package")
        if(DEFINED OPENTELEMETRY_VERSION AND OPENTELEMETRY_VERSION)
            message(STATUS "Using OpenTelemetry C++ version: ${OPENTELEMETRY_VERSION}")
            if(OPENTELEMETRY_VERSION VERSION_LESS MYIOT_OPENTELEMETRY_PACKAGE_VERSION)
                message(WARNING
                    "Found OpenTelemetry C++ ${OPENTELEMETRY_VERSION}, which is older than the default bootstrap version "
                    "${MYIOT_OPENTELEMETRY_PACKAGE_VERSION}."
                )
            endif()
        endif()

        if(_myiot_opentelemetry_optional_components)
            find_package(opentelemetry-cpp CONFIG QUIET COMPONENTS ${_myiot_opentelemetry_optional_components})
        endif()
    elseif(MYIOT_ENABLE_OPENTELEMETRY_BOOTSTRAP)
        find_package(Git REQUIRED)

        file(MAKE_DIRECTORY "${MYIOT_DEPS_DIR}")

        if(EXISTS "${MYIOT_OPENTELEMETRY_SOURCE_DIR}/CMakeLists.txt")
            message(STATUS "Using official OpenTelemetry C++ sources from: ${MYIOT_OPENTELEMETRY_SOURCE_DIR}")
        else()
            if(EXISTS "${MYIOT_OPENTELEMETRY_SOURCE_DIR}")
                get_filename_component(_myiot_opentelemetry_source_dir_real "${MYIOT_OPENTELEMETRY_SOURCE_DIR}" ABSOLUTE)
                get_filename_component(_myiot_opentelemetry_deps_dir_real "${MYIOT_DEPS_DIR}" ABSOLUTE)
                string(FIND
                    "${_myiot_opentelemetry_source_dir_real}"
                    "${_myiot_opentelemetry_deps_dir_real}"
                    _myiot_opentelemetry_source_dir_in_deps
                )

                if(_myiot_opentelemetry_source_dir_in_deps EQUAL 0)
                    message(STATUS "Removing incomplete OpenTelemetry C++ source tree: ${MYIOT_OPENTELEMETRY_SOURCE_DIR}")
                    file(REMOVE_RECURSE "${MYIOT_OPENTELEMETRY_SOURCE_DIR}")
                else()
                    message(FATAL_ERROR
                        "OpenTelemetry C++ source directory exists but does not look like a valid checkout: "
                        "${MYIOT_OPENTELEMETRY_SOURCE_DIR}. Remove it manually or point MYIOT_OPENTELEMETRY_SOURCE_DIR "
                        "at a valid opentelemetry-cpp source tree."
                    )
                endif()
            endif()

            message(STATUS "OpenTelemetry C++ was not found. Sources will be cloned under: ${MYIOT_OPENTELEMETRY_SOURCE_DIR}")
            execute_process(
                COMMAND
                    "${GIT_EXECUTABLE}" clone
                    "--branch" "${MYIOT_OPENTELEMETRY_VERSION}"
                    "--depth" "1"
                    "https://github.com/open-telemetry/opentelemetry-cpp.git"
                    "${MYIOT_OPENTELEMETRY_SOURCE_DIR}"
                RESULT_VARIABLE _myiot_opentelemetry_clone_result
                COMMAND_ERROR_IS_FATAL ANY
            )
        endif()

        set(_myiot_opentelemetry_had_build_testing_cache FALSE)
        if(DEFINED CACHE{BUILD_TESTING})
            set(_myiot_opentelemetry_had_build_testing_cache TRUE)
            get_property(_myiot_opentelemetry_build_testing_type CACHE BUILD_TESTING PROPERTY TYPE)
            get_property(_myiot_opentelemetry_build_testing_help CACHE BUILD_TESTING PROPERTY HELPSTRING)
            set(_myiot_opentelemetry_saved_build_testing "${BUILD_TESTING}")
        elseif(DEFINED BUILD_TESTING)
            set(_myiot_opentelemetry_saved_build_testing "${BUILD_TESTING}")
        else()
            set(_myiot_opentelemetry_saved_build_testing "__MYIOT_UNSET__")
        endif()

        set(OPENTELEMETRY_INSTALL ON CACHE BOOL "Install OpenTelemetry C++ when bootstrapped by MyIoT" FORCE)
        set(WITH_API_ONLY OFF CACHE BOOL "Build the OpenTelemetry C++ SDK" FORCE)
        set(WITH_ABI_VERSION_1 ON CACHE BOOL "Use the stable OpenTelemetry C++ ABI v1" FORCE)
        set(WITH_ABI_VERSION_2 OFF CACHE BOOL "Disable the experimental OpenTelemetry C++ ABI v2" FORCE)
        set(WITH_CONFIGURATION OFF CACHE BOOL "Disable OpenTelemetry declarative YAML configuration" FORCE)
        set(WITH_OTLP_GRPC OFF CACHE BOOL "Disable the OpenTelemetry OTLP gRPC exporter" FORCE)
        if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
            set(WITH_OTLP_HTTP ON CACHE BOOL "Enable the OpenTelemetry OTLP HTTP exporter" FORCE)
        else()
            set(WITH_OTLP_HTTP OFF CACHE BOOL "Disable the OpenTelemetry OTLP HTTP exporter" FORCE)
        endif()
        set(WITH_OTLP_FILE OFF CACHE BOOL "Disable the OpenTelemetry OTLP file exporter" FORCE)
        set(WITH_OTLP_HTTP_COMPRESSION OFF CACHE BOOL "Disable OTLP HTTP compression" FORCE)
        set(WITH_ZIPKIN OFF CACHE BOOL "Disable the OpenTelemetry Zipkin exporter" FORCE)
        set(WITH_PROMETHEUS OFF CACHE BOOL "Disable the OpenTelemetry Prometheus exporter" FORCE)
        set(WITH_ELASTICSEARCH OFF CACHE BOOL "Disable the OpenTelemetry Elasticsearch exporter" FORCE)
        set(WITH_OPENTRACING OFF CACHE BOOL "Disable the OpenTelemetry OpenTracing shim" FORCE)
        set(WITH_EXAMPLES OFF CACHE BOOL "Disable OpenTelemetry examples" FORCE)
        set(WITH_EXAMPLES_HTTP OFF CACHE BOOL "Disable OpenTelemetry HTTP examples" FORCE)
        set(WITH_FUNC_TESTS OFF CACHE BOOL "Disable OpenTelemetry functional tests" FORCE)
        set(BUILD_TESTING OFF CACHE BOOL "Disable OpenTelemetry tests while bootstrapping MyIoT" FORCE)
        set(WITH_BENCHMARK OFF CACHE BOOL "Disable OpenTelemetry benchmarks while bootstrapping MyIoT" FORCE)
        if(MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS)
            set(WITH_RESOURCE_DETECTORS_PREVIEW ON CACHE BOOL "Enable OpenTelemetry resource detectors" FORCE)
        else()
            set(WITH_RESOURCE_DETECTORS_PREVIEW OFF CACHE BOOL "Disable OpenTelemetry resource detectors" FORCE)
        endif()
        if(WIN32)
            set(WITH_ETW OFF CACHE BOOL "Disable the OpenTelemetry ETW exporter" FORCE)
        endif()

        message(STATUS "Bootstrapped OpenTelemetry artifacts will be installed to: ${CMAKE_INSTALL_PREFIX}")
        add_subdirectory("${MYIOT_OPENTELEMETRY_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/opentelemetry-cpp-official" EXCLUDE_FROM_ALL)
        set(MYIOT_BUNDLED_OPENTELEMETRY_INSTALL_SCRIPT "${CMAKE_BINARY_DIR}/opentelemetry-cpp-official/cmake_install.cmake")

        add_custom_target(myiot_opentelemetry_bootstrap ALL
            DEPENDS
                opentelemetry_sdk
                opentelemetry_common
                opentelemetry_resources
                opentelemetry_version
                opentelemetry_logs
                opentelemetry_trace
                opentelemetry_metrics
        )

        if(MYIOT_OPENTELEMETRY_ENABLE_OSTREAM_EXPORTER)
            add_dependencies(myiot_opentelemetry_bootstrap
                opentelemetry_exporter_ostream_span
                opentelemetry_exporter_ostream_metrics
                opentelemetry_exporter_ostream_logs
                opentelemetry_exporter_ostream_span_builder
                opentelemetry_exporter_ostream_metrics_builder
                opentelemetry_exporter_ostream_logs_builder
            )
        endif()

        add_dependencies(myiot_opentelemetry_bootstrap
            opentelemetry_exporter_in_memory
            opentelemetry_exporter_in_memory_metric
        )

        if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
            add_dependencies(myiot_opentelemetry_bootstrap
                opentelemetry_exporter_otlp_http_client
                opentelemetry_exporter_otlp_http
                opentelemetry_exporter_otlp_http_log
                opentelemetry_exporter_otlp_http_metric
            )
        endif()

        if(MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS)
            add_dependencies(myiot_opentelemetry_bootstrap
                opentelemetry_resource_detectors
            )
        endif()

        if(_myiot_opentelemetry_had_build_testing_cache)
            set(BUILD_TESTING "${_myiot_opentelemetry_saved_build_testing}"
                CACHE ${_myiot_opentelemetry_build_testing_type}
                "${_myiot_opentelemetry_build_testing_help}"
                FORCE
            )
        elseif(NOT _myiot_opentelemetry_saved_build_testing STREQUAL "__MYIOT_UNSET__")
            set(BUILD_TESTING "${_myiot_opentelemetry_saved_build_testing}")
            unset(BUILD_TESTING CACHE)
        else()
            unset(BUILD_TESTING CACHE)
            unset(BUILD_TESTING)
        endif()
    else()
        message(FATAL_ERROR "OpenTelemetry C++ was not found and MYIOT_ENABLE_OPENTELEMETRY_BOOTSTRAP is OFF.")
    endif()

    foreach(_myiot_opentelemetry_target IN LISTS _myiot_opentelemetry_imported_targets)
        myiot_normalize_imported_target_locations(${_myiot_opentelemetry_target})
    endforeach()

    add_library(myiot_opentelemetry INTERFACE)
    target_link_libraries(myiot_opentelemetry
        INTERFACE
            opentelemetry-cpp::api
            opentelemetry-cpp::sdk
            opentelemetry-cpp::trace
            opentelemetry-cpp::metrics
            opentelemetry-cpp::logs
            opentelemetry-cpp::resources
    )

    if(MYIOT_OPENTELEMETRY_ENABLE_RESOURCE_DETECTORS)
        if(TARGET opentelemetry-cpp::resource_detectors)
            add_library(myiot_opentelemetry_resource_detectors INTERFACE)
            target_link_libraries(myiot_opentelemetry_resource_detectors
                INTERFACE
                    myiot_opentelemetry
                    opentelemetry-cpp::resource_detectors
            )
        else()
            message(WARNING
                "OpenTelemetry resource detectors were requested, but the discovered package does not provide "
                "the resource_detectors component."
            )
        endif()
    endif()

    if(MYIOT_OPENTELEMETRY_ENABLE_OSTREAM_EXPORTER)
        if(TARGET opentelemetry-cpp::ostream_span_exporter)
            add_library(myiot_opentelemetry_ostream INTERFACE)
            target_link_libraries(myiot_opentelemetry_ostream
                INTERFACE
                    myiot_opentelemetry
                    opentelemetry-cpp::ostream_span_exporter
            )

            if(TARGET opentelemetry-cpp::ostream_metrics_exporter)
                target_link_libraries(myiot_opentelemetry_ostream
                    INTERFACE
                        opentelemetry-cpp::ostream_metrics_exporter
                )
            endif()

            if(TARGET opentelemetry-cpp::ostream_log_record_exporter)
                target_link_libraries(myiot_opentelemetry_ostream
                    INTERFACE
                        opentelemetry-cpp::ostream_log_record_exporter
                )
            endif()
        else()
            message(WARNING
                "OpenTelemetry ostream exporters were requested, but the discovered package does not provide "
                "the exporters_ostream component."
            )
        endif()
    endif()

    if(MYIOT_OPENTELEMETRY_ENABLE_OTLP_HTTP_EXPORTER)
        if(TARGET opentelemetry-cpp::otlp_http_exporter
           AND TARGET opentelemetry-cpp::otlp_http_log_record_exporter
           AND TARGET opentelemetry-cpp::otlp_http_metric_exporter
           AND TARGET opentelemetry-cpp::otlp_http_client)
            add_library(myiot_opentelemetry_otlp_http INTERFACE)
            target_link_libraries(myiot_opentelemetry_otlp_http
                INTERFACE
                    myiot_opentelemetry
                    opentelemetry-cpp::otlp_http_client
                    opentelemetry-cpp::otlp_http_exporter
                    opentelemetry-cpp::otlp_http_log_record_exporter
                    opentelemetry-cpp::otlp_http_metric_exporter
            )
        else()
            message(WARNING
                "OpenTelemetry OTLP HTTP exporters were requested, but the discovered package does not provide "
                "the exporters_otlp_http component."
            )
        endif()
    endif()
endif()

set(Poco_FOUND FALSE)
set(_myiot_local_poco_config_dir "${CMAKE_INSTALL_PREFIX}/cmake")
if(WIN32)
    set(_myiot_local_poco_foundation_debug "${CMAKE_INSTALL_PREFIX}/lib/PocoFoundationmdd.lib")
    set(_myiot_local_poco_foundation_release "${CMAKE_INSTALL_PREFIX}/lib/PocoFoundationmd.lib")
    set(_myiot_local_cppunit_debug "${CMAKE_INSTALL_PREFIX}/lib/CppUnitmdd.lib")
    set(_myiot_local_cppunit_release "${CMAKE_INSTALL_PREFIX}/lib/CppUnitmd.lib")
else()
    set(_myiot_local_poco_foundation_debug "${CMAKE_INSTALL_PREFIX}/lib/libPocoFoundationd.a")
    set(_myiot_local_poco_foundation_release "${CMAKE_INSTALL_PREFIX}/lib/libPocoFoundation.a")
    set(_myiot_local_cppunit_debug "${CMAKE_INSTALL_PREFIX}/lib/libCppUnitd.a")
    set(_myiot_local_cppunit_release "${CMAKE_INSTALL_PREFIX}/lib/libCppUnit.a")
endif()

if(EXISTS "${_myiot_local_poco_config_dir}/PocoConfig.cmake"
   AND EXISTS "${_myiot_local_poco_config_dir}/PocoCppUnitTargets.cmake"
   AND (EXISTS "${_myiot_local_poco_foundation_debug}" OR EXISTS "${_myiot_local_poco_foundation_release}")
   AND (EXISTS "${_myiot_local_cppunit_debug}" OR EXISTS "${_myiot_local_cppunit_release}"))
    set(Poco_DIR "${_myiot_local_poco_config_dir}")
    find_package(Poco CONFIG QUIET COMPONENTS ${MYIOT_POCO_COMPONENTS})

    if(Poco_FOUND)
        message(STATUS "Found official Poco package: ${Poco_DIR}")

        include("${_myiot_local_poco_config_dir}/PocoCppUnitTargets.cmake")
        set_target_properties(Poco::CppUnit PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/include"
            INTERFACE_LINK_LIBRARIES "Poco::Foundation"
        )

        foreach(_myiot_poco_target
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
        )
            myiot_normalize_imported_target_locations(${_myiot_poco_target})
        endforeach()

        if(MYIOT_ENABLE_DATA_MODULES)
            foreach(_myiot_poco_data_target
                Poco::Data
                Poco::DataSQLite
                Poco::ActiveRecord
            )
                myiot_normalize_imported_target_locations(${_myiot_poco_data_target})
            endforeach()
        endif()
    endif()
endif()

if(NOT Poco_FOUND AND MYIOT_ENABLE_POCO_BOOTSTRAP)
    find_package(Git REQUIRED)

    file(MAKE_DIRECTORY "${MYIOT_DEPS_DIR}")

    if(NOT EXISTS "${MYIOT_POCO_SOURCE_DIR}/CMakeLists.txt")
        message(STATUS "Official Poco was not found. Sources will be cloned under: ${MYIOT_POCO_SOURCE_DIR}")
        execute_process(
            COMMAND
                "${GIT_EXECUTABLE}" clone
                "--branch" "${MYIOT_POCO_VERSION}"
                "--depth" "1"
                "https://github.com/pocoproject/poco.git"
                "${MYIOT_POCO_SOURCE_DIR}"
            RESULT_VARIABLE _myiot_poco_clone_result
            COMMAND_ERROR_IS_FATAL ANY
        )
    else()
        message(STATUS "Using official Poco sources from: ${MYIOT_POCO_SOURCE_DIR}")
    endif()

    set(ENABLE_FOUNDATION ON CACHE BOOL "Enable Foundation" FORCE)
    set(ENABLE_ENCODINGS OFF CACHE BOOL "Disable Encodings" FORCE)
    set(ENABLE_ENCODINGS_COMPILER OFF CACHE BOOL "Disable Encodings Compiler" FORCE)
    set(ENABLE_XML ON CACHE BOOL "Enable XML" FORCE)
    set(ENABLE_JSON ON CACHE BOOL "Enable JSON" FORCE)
    set(ENABLE_MONGODB OFF CACHE BOOL "Disable MongoDB" FORCE)
    if(MYIOT_ENABLE_DATA_MODULES)
        set(ENABLE_DATA ON CACHE BOOL "Enable Data" FORCE)
        set(ENABLE_DATA_SQLITE ON CACHE BOOL "Enable Data SQLite" FORCE)
    else()
        set(ENABLE_DATA OFF CACHE BOOL "Disable Data" FORCE)
        set(ENABLE_DATA_SQLITE OFF CACHE BOOL "Disable Data SQLite" FORCE)
    endif()
    set(ENABLE_REDIS ON CACHE BOOL "Enable Redis" FORCE)
    set(ENABLE_PDF OFF CACHE BOOL "Disable PDF" FORCE)
    set(ENABLE_UTIL ON CACHE BOOL "Enable Util" FORCE)
    set(ENABLE_NET ON CACHE BOOL "Enable Net" FORCE)
    set(ENABLE_SEVENZIP OFF CACHE BOOL "Disable SevenZip" FORCE)
    set(ENABLE_ZIP ON CACHE BOOL "Enable Zip" FORCE)
    set(ENABLE_CPPPARSER ON CACHE BOOL "Enable CppParser" FORCE)
    set(ENABLE_POCODOC OFF CACHE BOOL "Disable PocoDoc" FORCE)
    set(ENABLE_PAGECOMPILER ON CACHE BOOL "Enable PageCompiler" FORCE)
    set(ENABLE_PAGECOMPILER_FILE2PAGE ON CACHE BOOL "Enable File2Page" FORCE)
    if(MYIOT_ENABLE_DATA_MODULES)
        set(ENABLE_ACTIVERECORD ON CACHE BOOL "Enable ActiveRecord" FORCE)
    else()
        set(ENABLE_ACTIVERECORD OFF CACHE BOOL "Disable ActiveRecord" FORCE)
    endif()
    set(ENABLE_ACTIVERECORD_COMPILER OFF CACHE BOOL "Disable ActiveRecord compiler" FORCE)
    set(ENABLE_NETSSL_WIN OFF CACHE BOOL "Disable NetSSL_Win" FORCE)
    set(ENABLE_NETSSL ON CACHE BOOL "Enable NetSSL" FORCE)
    set(ENABLE_CRYPTO ON CACHE BOOL "Enable Crypto" FORCE)
    set(ENABLE_JWT ON CACHE BOOL "Enable JWT" FORCE)
    set(ENABLE_APACHECONNECTOR OFF CACHE BOOL "Disable ApacheConnector" FORCE)
    set(ENABLE_TESTS ON CACHE BOOL "Enable CppUnit and official Poco test trees" FORCE)

    message(STATUS "Bootstrapped Poco artifacts will be installed to: ${CMAKE_INSTALL_PREFIX}")
    add_subdirectory("${MYIOT_POCO_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/poco-official" EXCLUDE_FROM_ALL)
    set(MYIOT_BUNDLED_POCO_INSTALL_SCRIPT "${CMAKE_BINARY_DIR}/poco-official/cmake_install.cmake")

    include(CMakePackageConfigHelpers)
    set(_myiot_cppunit_config_dir "${CMAKE_BINARY_DIR}/myiot-poco-cppunit")
    file(MAKE_DIRECTORY "${_myiot_cppunit_config_dir}")
    file(WRITE "${_myiot_cppunit_config_dir}/PocoCppUnitConfig.cmake"
        "include(\"${CMAKE_CURRENT_LIST_DIR}/PocoCppUnitTargets.cmake\")\n"
    )
    write_basic_package_version_file(
        "${_myiot_cppunit_config_dir}/PocoCppUnitConfigVersion.cmake"
        VERSION "${MYIOT_POCO_PACKAGE_VERSION}"
        COMPATIBILITY AnyNewerVersion
    )

    install(DIRECTORY "${MYIOT_POCO_SOURCE_DIR}/CppUnit/include/CppUnit"
        DESTINATION include
        COMPONENT Devel
    )
    install(TARGETS CppUnit EXPORT PocoCppUnitTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(EXPORT PocoCppUnitTargets
        FILE PocoCppUnitTargets.cmake
        NAMESPACE Poco::
        DESTINATION cmake
    )
    install(FILES
        "${_myiot_cppunit_config_dir}/PocoCppUnitConfig.cmake"
        "${_myiot_cppunit_config_dir}/PocoCppUnitConfigVersion.cmake"
        DESTINATION cmake
    )

    add_custom_target(myiot_poco_bootstrap ALL
        DEPENDS
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
            CppUnit
            PageCompiler
            File2Page
    )
    if(MYIOT_ENABLE_DATA_MODULES)
        add_dependencies(myiot_poco_bootstrap
            Data
            DataSQLite
            ActiveRecord
        )
    endif()
elseif(NOT Poco_FOUND)
    message(FATAL_ERROR "Official Poco was not found and MYIOT_ENABLE_POCO_BOOTSTRAP is OFF.")
endif()

if(DEFINED MYIOT_BUNDLED_POCO_INSTALL_SCRIPT)
    install(CODE
        "execute_process(
            COMMAND \"${CMAKE_COMMAND}\"
                -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                -DCMAKE_INSTALL_CONFIG_NAME=\${CMAKE_INSTALL_CONFIG_NAME}
                -P \"${MYIOT_BUNDLED_POCO_INSTALL_SCRIPT}\"
            COMMAND_ERROR_IS_FATAL ANY
        )"
    )
endif()

if(DEFINED MYIOT_BUNDLED_OPENTELEMETRY_INSTALL_SCRIPT)
    install(CODE
        "execute_process(
            COMMAND \"${CMAKE_COMMAND}\"
                -DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
                -DCMAKE_INSTALL_CONFIG_NAME=\${CMAKE_INSTALL_CONFIG_NAME}
                -P \"${MYIOT_BUNDLED_OPENTELEMETRY_INSTALL_SCRIPT}\"
            COMMAND_ERROR_IS_FATAL ANY
        )"
    )
endif()

if(WIN32)
    get_filename_component(MYIOT_OPENSSL_ROOT "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
    set(MYIOT_OPENSSL_BIN_DIR "${MYIOT_OPENSSL_ROOT}/bin")
    file(GLOB MYIOT_OPENSSL_RUNTIME_DLLS
        "${MYIOT_OPENSSL_ROOT}/libcrypto*.dll"
        "${MYIOT_OPENSSL_ROOT}/libssl*.dll"
        "${MYIOT_OPENSSL_BIN_DIR}/libcrypto*.dll"
        "${MYIOT_OPENSSL_BIN_DIR}/libssl*.dll"
    )
    list(REMOVE_DUPLICATES MYIOT_OPENSSL_RUNTIME_DLLS)
    set(MYIOT_OPENSSL_LEGACY_PROVIDER "${MYIOT_OPENSSL_BIN_DIR}/legacy.dll")
    set(MYIOT_OPENSSL_CONFIG_FILE "${MYIOT_OPENSSL_BIN_DIR}/cnf/openssl.cnf")
    set(MYIOT_OPENSSL_RUNTIME_DLLS "${MYIOT_OPENSSL_RUNTIME_DLLS}" CACHE INTERNAL "OpenSSL runtime DLLs for Windows")
    set(MYIOT_OPENSSL_LEGACY_PROVIDER "${MYIOT_OPENSSL_LEGACY_PROVIDER}" CACHE INTERNAL "OpenSSL legacy provider DLL for Windows")
    set(MYIOT_OPENSSL_CONFIG_FILE "${MYIOT_OPENSSL_CONFIG_FILE}" CACHE INTERNAL "OpenSSL config file for Windows")
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
