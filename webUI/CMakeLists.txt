find_program(MYIOT_NPM_EXECUTABLE npm REQUIRED)

set(MYIOT_WEBUI_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

include(CMakeParseArguments)

function(myiot_add_webui_package)
    set(options)
    set(oneValueArgs TARGET_NAME BUNDLE_NAME PACKAGE_VERSION UI_DIR NATIVE_DIR BUNDLE_SPEC_TEMPLATE)
    set(multiValueArgs SOURCES LINK_LIBS INCLUDE_DIRS)
    cmake_parse_arguments(WEBUI "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT WEBUI_TARGET_NAME OR NOT WEBUI_BUNDLE_NAME OR NOT WEBUI_UI_DIR OR NOT WEBUI_NATIVE_DIR OR NOT WEBUI_BUNDLE_SPEC_TEMPLATE)
        message(FATAL_ERROR "myiot_add_webui_package requires TARGET_NAME, BUNDLE_NAME, UI_DIR, NATIVE_DIR and BUNDLE_SPEC_TEMPLATE.")
    endif()

    if(NOT WEBUI_PACKAGE_VERSION)
        set(WEBUI_PACKAGE_VERSION "0.1.0")
    endif()

    set(stage_dir "${CMAKE_CURRENT_BINARY_DIR}/stage")
    set(stage_bundle_dir "${stage_dir}/bundle")
    set(stage_webapp_dir "${stage_bundle_dir}/webapp")
    set(stage_code_dir "${stage_dir}/bin/${MYIOT_PLATFORM_OSNAME}/${MYIOT_PLATFORM_OSARCH}")
    set(bundle_spec "${CMAKE_CURRENT_BINARY_DIR}/${WEBUI_TARGET_NAME}.bndlspec")
    set(bundle_file "${CMAKE_BINARY_DIR}/bundles/${WEBUI_BUNDLE_NAME}_${WEBUI_PACKAGE_VERSION}.bndl")
    set(ui_manifest_files "${WEBUI_UI_DIR}/package.json")

    if(EXISTS "${WEBUI_UI_DIR}/package-lock.json")
        list(APPEND ui_manifest_files "${WEBUI_UI_DIR}/package-lock.json")
    endif()

    file(GLOB_RECURSE frontend_sources CONFIGURE_DEPENDS
        "${WEBUI_UI_DIR}/src/*"
        "${WEBUI_UI_DIR}/public/*"
        "${WEBUI_UI_DIR}/index.html"
        ${ui_manifest_files}
        "${WEBUI_UI_DIR}/vite.config.mjs"
        "${MYIOT_WEBUI_ROOT_DIR}/*/bundle.json"
    )

    set(MYIOT_WEBUI_PACKAGE_BUNDLE_NAME "${WEBUI_BUNDLE_NAME}")
    set(MYIOT_WEBUI_PACKAGE_VERSION "${WEBUI_PACKAGE_VERSION}")
    configure_file("${WEBUI_BUNDLE_SPEC_TEMPLATE}" "${bundle_spec}" @ONLY)

    if(WIN32)
        set(npm_install_command cmd /c call "${MYIOT_NPM_EXECUTABLE}" install)
        set(npm_build_command cmd /c call "${MYIOT_NPM_EXECUTABLE}" run build)
    else()
        set(npm_install_command "${MYIOT_NPM_EXECUTABLE}" install)
        set(npm_build_command "${MYIOT_NPM_EXECUTABLE}" run build)
    endif()

    add_custom_target(${WEBUI_TARGET_NAME}_UIInstall
        COMMAND ${npm_install_command}
        WORKING_DIRECTORY "${WEBUI_UI_DIR}"
        DEPENDS ${ui_manifest_files}
        VERBATIM
    )

    add_custom_target(${WEBUI_TARGET_NAME}_Frontend ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_bundle_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WEBUI_NATIVE_DIR}/bundle/extensions.xml" "${stage_bundle_dir}/extensions.xml"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${WEBUI_NATIVE_DIR}/bundle/bundle.properties" "${stage_bundle_dir}/bundle.properties"
        COMMAND ${npm_build_command}
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${stage_webapp_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_webapp_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${WEBUI_UI_DIR}/dist" "${stage_webapp_dir}"
        WORKING_DIRECTORY "${WEBUI_UI_DIR}"
        DEPENDS ${frontend_sources} "${CMAKE_CURRENT_LIST_FILE}"
        VERBATIM
    )
    add_dependencies(${WEBUI_TARGET_NAME}_Frontend ${WEBUI_TARGET_NAME}_UIInstall)

    add_library(${WEBUI_TARGET_NAME} SHARED ${WEBUI_SOURCES})
    set_target_properties(${WEBUI_TARGET_NAME} PROPERTIES OUTPUT_NAME ${WEBUI_BUNDLE_NAME} PREFIX "" DEBUG_POSTFIX d)
    target_include_directories(${WEBUI_TARGET_NAME} PRIVATE ${MYIOT_OSP_BUNDLE_INCLUDE_DIRS} ${WEBUI_INCLUDE_DIRS})
    target_link_libraries(${WEBUI_TARGET_NAME} PRIVATE ${WEBUI_LINK_LIBS})
    add_dependencies(${WEBUI_TARGET_NAME} ${WEBUI_TARGET_NAME}_Frontend)

    set(bundle_stage_commands
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_code_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${WEBUI_TARGET_NAME}>" "${stage_code_dir}/"
    )
    if(MSVC)
        list(APPEND bundle_stage_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${WEBUI_TARGET_NAME}>" "${stage_code_dir}/"
        )
    endif()

    add_custom_target(${WEBUI_TARGET_NAME}_Bundle ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bundles"
        ${bundle_stage_commands}
        COMMAND ${MYIOT_OSP_BUNDLE_CREATOR_COMMAND} "--output-dir=${CMAKE_BINARY_DIR}/bundles" "--osname=${MYIOT_PLATFORM_OSNAME}" "--osarch=${MYIOT_PLATFORM_OSARCH}" "${bundle_spec}"
        DEPENDS
            OSPBundleCreator
            ${WEBUI_TARGET_NAME}_Frontend
            ${WEBUI_TARGET_NAME}
            ${frontend_sources}
            "${bundle_spec}"
            "${WEBUI_NATIVE_DIR}/bundle/extensions.xml"
            "${WEBUI_NATIVE_DIR}/bundle/bundle.properties"
            "$<TARGET_FILE:OSPBundleCreator>"
            "$<TARGET_FILE:${WEBUI_TARGET_NAME}>"
        WORKING_DIRECTORY "${stage_dir}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )
    add_dependencies(${WEBUI_TARGET_NAME}_Bundle ${WEBUI_TARGET_NAME})

    install(TARGETS ${WEBUI_TARGET_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
    install(CODE "file(REMOVE \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/bundles/${WEBUI_BUNDLE_NAME}_${WEBUI_PACKAGE_VERSION}.bndl\")")
    install(FILES "${bundle_file}" DESTINATION ${CMAKE_INSTALL_LIBDIR}/bundles)
endfunction()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Launcher" "${CMAKE_CURRENT_BINARY_DIR}/Launcher")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/Home" "${CMAKE_CURRENT_BINARY_DIR}/Home")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/LogViewer" "${CMAKE_CURRENT_BINARY_DIR}/LogViewer")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/BundleList" "${CMAKE_CURRENT_BINARY_DIR}/BundleList")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/SystemMonitor" "${CMAKE_CURRENT_BINARY_DIR}/SystemMonitor")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/ProcessConsole" "${CMAKE_CURRENT_BINARY_DIR}/ProcessConsole")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/JNDM123" "${CMAKE_CURRENT_BINARY_DIR}/JNDM123")
else()
    message(STATUS "Skipping webUI/JNDM123 because it is only supported for Linux targets.")
endif()
