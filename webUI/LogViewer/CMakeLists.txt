myiot_add_webui_package(
    TARGET_NAME MyIoTLogViewer
    BUNDLE_NAME io.myiot.webui.logviewer
    PACKAGE_VERSION 0.1.0
    UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui
    NATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/LogViewer.bndlspec.in
    SOURCES
        src/BundleActivator.cpp
    LINK_LIBS
        Poco::OSP
        Poco::OSPWeb
        Poco::Foundation
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
