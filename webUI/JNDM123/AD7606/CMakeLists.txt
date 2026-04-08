if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
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
