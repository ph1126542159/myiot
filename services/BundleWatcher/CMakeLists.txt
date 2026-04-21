set(myiot_bundle_watcher_link_libs
    Poco::OSP
    Poco::Net
    Poco::Util
    Poco::Foundation
)

myiot_add_service_bundle(
    TARGET_NAME MyIoTBundleWatcher
    BUNDLE_NAME io.myiot.services.bundlewatcher
    PACKAGE_VERSION 0.1.0
    BUNDLE_SPEC_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/BundleWatcher.bndlspec.in
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleActivator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/BundleWatcherService.cpp
    LINK_LIBS
        ${myiot_bundle_watcher_link_libs}
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
