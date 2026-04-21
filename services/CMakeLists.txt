include(CMakeParseArguments)

if(WIN32)
    set(MYIOT_BUNDLE_OPT_PREFIX "/")
else()
    set(MYIOT_BUNDLE_OPT_PREFIX "--")
endif()

function(myiot_add_service_bundle)
    set(options)
    set(oneValueArgs TARGET_NAME BUNDLE_NAME PACKAGE_VERSION BUNDLE_SPEC_TEMPLATE)
    set(multiValueArgs SOURCES LINK_LIBS INCLUDE_DIRS)
    cmake_parse_arguments(SVC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT SVC_TARGET_NAME OR NOT SVC_BUNDLE_NAME OR NOT SVC_BUNDLE_SPEC_TEMPLATE)
        message(FATAL_ERROR "myiot_add_service_bundle requires TARGET_NAME, BUNDLE_NAME and BUNDLE_SPEC_TEMPLATE.")
    endif()

    if(NOT SVC_PACKAGE_VERSION)
        set(SVC_PACKAGE_VERSION "0.1.0")
    endif()

    if(TARGET Poco::OpenTelemetry)
        list(APPEND SVC_LINK_LIBS Poco::OpenTelemetry)
    endif()

    set(bundle_spec "${CMAKE_CURRENT_BINARY_DIR}/${SVC_TARGET_NAME}.bndlspec")
    set(bundle_file "${CMAKE_BINARY_DIR}/bundles/${SVC_BUNDLE_NAME}_${SVC_PACKAGE_VERSION}.bndl")
    set(MYIOT_SERVICE_BUNDLE_NAME "${SVC_BUNDLE_NAME}")
    set(MYIOT_SERVICE_PACKAGE_VERSION "${SVC_PACKAGE_VERSION}")
    configure_file("${SVC_BUNDLE_SPEC_TEMPLATE}" "${bundle_spec}" @ONLY)

    add_library(${SVC_TARGET_NAME} SHARED ${SVC_SOURCES})
    set_target_properties(${SVC_TARGET_NAME}
        PROPERTIES
            OUTPUT_NAME ${SVC_BUNDLE_NAME}
            PREFIX ""
            DEBUG_POSTFIX d
    )
    target_link_libraries(${SVC_TARGET_NAME} PRIVATE ${SVC_LINK_LIBS})
    if(SVC_INCLUDE_DIRS)
        target_include_directories(${SVC_TARGET_NAME} PRIVATE ${SVC_INCLUDE_DIRS})
    endif()

    set(bundle_resource_deps)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bundle")
        file(GLOB_RECURSE bundle_resource_deps CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/bundle/*")
    endif()

    file(RELATIVE_PATH bundle_code_rel_dir "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/bin")
    if(MSVC)
        set(bundle_code_args
            "${MYIOT_BUNDLE_OPT_PREFIX}code=${bundle_code_rel_dir}/$<TARGET_FILE_NAME:${SVC_TARGET_NAME}>"
        )
    else()
        set(bundle_code_args "${MYIOT_BUNDLE_OPT_PREFIX}code=${bundle_code_rel_dir}/$<TARGET_FILE_NAME:${SVC_TARGET_NAME}>*")
    endif()

    add_custom_target(${SVC_TARGET_NAME}_Bundle ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bundles"
        COMMAND ${MYIOT_OSP_BUNDLE_CREATOR_COMMAND}
            "${MYIOT_BUNDLE_OPT_PREFIX}output-dir=${CMAKE_BINARY_DIR}/bundles"
            "${MYIOT_BUNDLE_OPT_PREFIX}osname=${MYIOT_PLATFORM_OSNAME}"
            "${MYIOT_BUNDLE_OPT_PREFIX}osarch=${MYIOT_PLATFORM_OSARCH}"
            ${bundle_code_args}
            "${bundle_spec}"
        DEPENDS
            OSPBundleCreator
            ${SVC_TARGET_NAME}
            "${bundle_spec}"
            ${bundle_resource_deps}
            "$<TARGET_FILE:OSPBundleCreator>"
            "$<TARGET_FILE:${SVC_TARGET_NAME}>"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    add_dependencies(${SVC_TARGET_NAME}_Bundle ${SVC_TARGET_NAME})

    install(TARGETS ${SVC_TARGET_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    install(CODE "file(REMOVE \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/bundles/${SVC_BUNDLE_NAME}_${SVC_PACKAGE_VERSION}.bndl\")")
    install(FILES "${bundle_file}" DESTINATION ${CMAKE_INSTALL_LIBDIR}/bundles)
endfunction()

add_subdirectory(ProcessConsole)
add_subdirectory(BundleWatcher)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory(JNDM123AcquisitionAgent)
else()
    message(STATUS "Skipping services/JNDM123AcquisitionAgent because it is only supported for Linux targets.")
endif()
