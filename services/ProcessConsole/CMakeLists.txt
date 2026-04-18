set(myiot_process_console_link_libs
    Poco::OSP
    Poco::OSPWeb
    Poco::JSON
    Poco::Net
    Poco::Util
    Poco::Foundation
)

if(WIN32)
    list(APPEND myiot_process_console_link_libs Dbghelp)
endif()

myiot_add_service_bundle(
    TARGET_NAME MyIoTProcessConsole
    BUNDLE_NAME io.myiot.services.processconsole
    PACKAGE_VERSION 0.1.0
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/ProcessConsole.bndlspec.in
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleActivator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ConsoleCommandRequestHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ProcessConsoleService.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/TerminalWebSocketRequestHandler.cpp
    LINK_LIBS
        ${myiot_process_console_link_libs}
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
