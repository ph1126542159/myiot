myiot_add_osp_bundle(OSP_SecureWebServer_Bundle "osp.web.server.secure"
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/SecureWebServerBundleActivator.cpp
    LINK_LIBS Poco::OSPWeb Poco::OSP Poco::NetSSL Poco::Net Poco::Util Poco::XML Poco::Foundation
    INCLUDE_DIRS
        ${MYIOT_OSP_BUNDLE_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/../Web/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../Web/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    BUNDLE_SPEC ${CMAKE_CURRENT_SOURCE_DIR}/SecureWebServer.bndlspec
)
