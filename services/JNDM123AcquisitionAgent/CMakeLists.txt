find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(myiot_fastdds_prefix "/home/ph/work/myiot/build/install")
set(myiot_fastdds_dir "${myiot_fastdds_prefix}/share/fastdds/cmake")
set(myiot_fastcdr_dir "${myiot_fastdds_prefix}/lib/cmake/fastcdr")
set(myiot_foonathan_dir "${myiot_fastdds_prefix}/lib/foonathan_memory/cmake")

if(NOT fastdds_FOUND)
    if(EXISTS "${myiot_fastdds_dir}/fastdds-config.cmake")
        set(fastdds_DIR "${myiot_fastdds_dir}")
    endif()
    if(EXISTS "${myiot_fastcdr_dir}/fastcdr-config.cmake")
        set(fastcdr_DIR "${myiot_fastcdr_dir}")
    endif()
    if(EXISTS "${myiot_foonathan_dir}/foonathan_memory-config.cmake")
        set(foonathan_memory_DIR "${myiot_foonathan_dir}")
    endif()
    find_package(fastdds CONFIG REQUIRED)
endif()

set(myiot_jndm123_agent_sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionAgentSupport.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionDdsJsonMessage.cpp
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AcquisitionRuntime.cpp
)
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

install(FILES
    "${myiot_fastdds_prefix}/lib/libfastdds.so.3.6.0.0"
    "${myiot_fastdds_prefix}/lib/libfastcdr.so.2.3.5"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(CODE [[
    file(CREATE_LINK "libfastdds.so.3.6.0.0" "${CMAKE_INSTALL_PREFIX}/lib/libfastdds.so.3.6" SYMBOLIC)
    file(CREATE_LINK "libfastdds.so.3.6" "${CMAKE_INSTALL_PREFIX}/lib/libfastdds.so" SYMBOLIC)
    file(CREATE_LINK "libfastcdr.so.2.3.5" "${CMAKE_INSTALL_PREFIX}/lib/libfastcdr.so.2" SYMBOLIC)
    file(CREATE_LINK "libfastcdr.so.2" "${CMAKE_INSTALL_PREFIX}/lib/libfastcdr.so" SYMBOLIC)
]])
