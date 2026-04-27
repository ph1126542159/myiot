find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

if(NOT TARGET fastdds)
    find_package(fastdds CONFIG REQUIRED)
endif()

set(myiot_jndm123_agent_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionAgentSupport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionDdsJsonMessage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionUdpStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JNDM123AcquisitionServiceImpl.cpp
)

set(myiot_jndm123_agent_link_libs
    Poco::OSP
    Poco::JSON
    Poco::Net
    Poco::Foundation
    fastdds
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

add_library(MyIoTJNDM123AcquisitionApi SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/src/JNDM123AcquisitionServiceApi.cpp
)
target_link_libraries(MyIoTJNDM123AcquisitionApi PUBLIC
    Poco::OSP
    Poco::JSON
    Poco::Foundation
)
target_include_directories(MyIoTJNDM123AcquisitionApi PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set_target_properties(MyIoTJNDM123AcquisitionApi PROPERTIES
    OUTPUT_NAME myiot-jndm123-acquisition-api
)

myiot_add_service_bundle(
    TARGET_NAME MyIoTJNDM123AcquisitionService
    BUNDLE_NAME io.myiot.services.jndm123acquisition
    PACKAGE_VERSION 0.1.0
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/JNDM123AcquisitionAgent.bndlspec.in
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleActivator.cpp
        ${myiot_jndm123_agent_sources}
    LINK_LIBS
        MyIoTJNDM123AcquisitionApi
        ${myiot_jndm123_agent_link_libs}
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(myiot-jndm123-acquisition-agent
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionAgentApp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionAgentSupport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionDivider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionDividerState.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionDdsJsonMessage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionUdpStream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionRuntime.cpp
)
myiot_copy_windows_runtime_dlls(myiot-jndm123-acquisition-agent)
target_link_libraries(myiot-jndm123-acquisition-agent PRIVATE
    Poco::JSON
    Poco::Net
    Poco::Util
    Poco::Foundation
    fastdds
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)
target_include_directories(myiot-jndm123-acquisition-agent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

install(TARGETS myiot-jndm123-acquisition-agent
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(TARGETS MyIoTJNDM123AcquisitionApi
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY "${MYIOT_FASTDDS_PREFIX}/lib/"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILES_MATCHING
        PATTERN "libfastdds.so*"
        PATTERN "libfastcdr.so*"
)
