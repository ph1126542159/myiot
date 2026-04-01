myiot_add_webui_package(
    TARGET_NAME MyIoTGlobalConfig
    BUNDLE_NAME io.myiot.webui.config
    PACKAGE_VERSION 0.1.0
    UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui
    NATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/GlobalConfig.bndlspec.in
    SOURCES
        src/BundleActivator.cpp
        src/GlobalConfigRequestHandler.cpp
    LINK_LIBS
        Poco::OSP
        Poco::OSPWeb
        Poco::JSON
        Poco::Net
        Poco::Util
        Poco::Foundation
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
