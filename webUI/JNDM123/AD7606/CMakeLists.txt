if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    find_package(OpenSSL REQUIRED)

    if(NOT TARGET fastdds)
        find_package(fastdds CONFIG REQUIRED)
    endif()

    myiot_add_webui_package(
        TARGET_NAME MyIoTJNDM123
        BUNDLE_NAME io.myiot.webui.jndm123
        PACKAGE_VERSION 0.1.0
        UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui
        NATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
        BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/JNDM123.bndlspec.in
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleActivator.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/JNDM123AgentManager.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/JNDM123DdsBridge.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/JNDM123RequestHandler.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/JNDM123Runtime.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/JNDM123Support.cpp
            ${CMAKE_SOURCE_DIR}/services/JNDM123AcquisitionAgent/src/AcquisitionDdsJsonMessage.cpp
        LINK_LIBS
            MyIoTJNDM123AcquisitionApi
            Poco::OSP
            Poco::OSPWeb
            Poco::JSON
            Poco::Net
            Poco::Util
            Poco::Foundation
            fastdds
            OpenSSL::SSL
            OpenSSL::Crypto
            Threads::Threads
        INCLUDE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/services/JNDM123AcquisitionAgent/src
    )
endif()
