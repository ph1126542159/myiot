myiot_add_webui_package(
    TARGET_NAME MyIoTLauncher
    BUNDLE_NAME io.myiot.webui.launcher
    PACKAGE_VERSION 0.1.0
    UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui
    NATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/Launcher.bndlspec.in
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AuthRequestHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleActivator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/RootRedirectRequestHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/SessionJSON.cpp
    LINK_LIBS
        Poco::OSPWeb
        Poco::OSP
        Poco::JSON
        Poco::Net
        Poco::Util
        Poco::XML
        Poco::Foundation
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
