find_package(Threads REQUIRED)

myiot_add_webui_package(
    TARGET_NAME MyIoTJNDM123
    BUNDLE_NAME io.myiot.webui.jndm123
    PACKAGE_VERSION 0.1.0
    UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui
    NATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/JNDM123.bndlspec.in
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleActivator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/JNDM123RequestHandler.cpp
    LINK_LIBS
        Poco::OSP
        Poco::OSPWeb
        Poco::JSON
        Poco::Net
        Poco::Util
        Poco::Foundation
        Threads::Threads
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
