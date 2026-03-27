set(SRCS)
add_library(OSPWebEvent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WebEventService.cpp
)
add_library(Poco::OSPWebEvent ALIAS OSPWebEvent)
set_target_properties(OSPWebEvent
    PROPERTIES
    VERSION "2" SOVERSION "2"
    OUTPUT_NAME PocoOSPWebEvent
    DEFINE_SYMBOL OSPWebEvent_EXPORTS
)

target_link_libraries(OSPWebEvent PUBLIC Poco::OSPWeb Poco::OSP Poco::Net Poco::Util Poco::XML Poco::Foundation)
target_include_directories(OSPWebEvent
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

myiot_add_osp_bundle(OSP_WebEvent_Bundle "com.appinf.osp.webevent"
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/WebEventBundleActivator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/WebEventRequestHandler.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/WebEventServiceImpl.cpp
    LINK_LIBS Poco::OSPWebEvent Poco::OSPWeb Poco::OSP Poco::Net Poco::Util Poco::XML Poco::Foundation
    INCLUDE_DIRS
        ${MYIOT_OSP_BUNDLE_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    BUNDLE_SPEC ${CMAKE_CURRENT_SOURCE_DIR}/WebEventBundle.bndlspec
)
