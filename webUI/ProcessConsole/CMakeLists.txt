myiot_add_webui_package(
    TARGET_NAME MyIoTProcessConsoleUI
    BUNDLE_NAME io.myiot.webui.console
    PACKAGE_VERSION 0.1.0
    UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui
    NATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/ProcessConsole.bndlspec.in
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleActivator.cpp
    LINK_LIBS
        Poco::OSP
        Poco::Net
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
