set(myiot_system_monitor_link_libs
    Poco::OSP
    Poco::OSPWeb
    Poco::JSON
    Poco::Net
    Poco::Util
    Poco::Foundation
)

if(TARGET Poco::OpenTelemetry)
    list(APPEND myiot_system_monitor_link_libs Poco::OpenTelemetry)
endif()

if(WIN32)
    list(APPEND myiot_system_monitor_link_libs
        Iphlpapi
        Pdh
    )
endif()

myiot_add_webui_package(
    TARGET_NAME MyIoTSystemMonitor
    BUNDLE_NAME io.myiot.webui.monitor
    PACKAGE_VERSION 0.1.0
    UI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ui
    NATIVE_DIR ${CMAKE_CURRENT_SOURCE_DIR}
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/SystemMonitor.bndlspec.in
    SOURCES
        src/BundleActivator.cpp
        src/SystemMetricsRequestHandler.cpp
        src/TelemetrySnapshotRequestHandler.cpp
    LINK_LIBS
        ${myiot_system_monitor_link_libs}
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
