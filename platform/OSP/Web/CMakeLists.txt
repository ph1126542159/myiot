# Sources
set(SRCS)
file(GLOB SRCS_G "src/*.cpp")
list(REMOVE_ITEM SRCS_G "${CMAKE_CURRENT_SOURCE_DIR}/src/WebBundleActivator.cpp")
POCO_SOURCES_AUTO(SRCS ${SRCS_G})

# Headers
set(HDRS_G)
file(GLOB_RECURSE HDRS_G "include/*.h")
POCO_HEADERS_AUTO(SRCS ${HDRS_G})

add_library(OSPWeb ${SRCS})
add_library(Poco::OSPWeb ALIAS OSPWeb)
set_target_properties(OSPWeb
    PROPERTIES
    VERSION "3" SOVERSION "3"
    OUTPUT_NAME PocoOSPWeb
    DEFINE_SYMBOL OSPWeb_EXPORTS
)

target_link_libraries(OSPWeb PUBLIC Poco::OSP Poco::Net Poco::Util Poco::XML Poco::Foundation)
target_include_directories(OSPWeb
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

myiot_add_osp_bundle(OSP_Web_Bundle "osp.web"
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/WebBundleActivator.cpp
    LINK_LIBS Poco::OSPWeb Poco::OSP Poco::Net Poco::Util Poco::XML Poco::Foundation
    INCLUDE_DIRS
        ${MYIOT_OSP_BUNDLE_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    BUNDLE_SPEC ${CMAKE_CURRENT_SOURCE_DIR}/WebBundle.bndlspec
)
