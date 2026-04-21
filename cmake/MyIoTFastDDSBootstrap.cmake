include("${CMAKE_CURRENT_LIST_DIR}/MyIoTBootstrapHelpers.cmake")

myiot_bootstrap_require(MYIOT_BOOTSTRAP_DEPS_DIR)
myiot_bootstrap_require(MYIOT_FASTDDS_PREFIX)
myiot_bootstrap_require(MYIOT_FASTDDS_VERSION)
myiot_bootstrap_require(MYIOT_FASTCDR_VERSION)
myiot_bootstrap_require(MYIOT_FOONATHAN_MEMORY_VENDOR_VERSION)

set(_myiot_foonathan_source_dir "${MYIOT_BOOTSTRAP_DEPS_DIR}/foonathan_memory_vendor")
set(_myiot_fastcdr_source_dir "${MYIOT_BOOTSTRAP_DEPS_DIR}/Fast-CDR")
set(_myiot_fastdds_source_dir "${MYIOT_BOOTSTRAP_DEPS_DIR}/Fast-DDS")

myiot_bootstrap_clone_if_missing(
    "${_myiot_foonathan_source_dir}"
    "https://github.com/eProsima/foonathan_memory_vendor.git"
    "${MYIOT_FOONATHAN_MEMORY_VENDOR_VERSION}"
    "foonathan_memory_vendor"
)
myiot_bootstrap_clone_if_missing(
    "${_myiot_fastcdr_source_dir}"
    "https://github.com/eProsima/Fast-CDR.git"
    "${MYIOT_FASTCDR_VERSION}"
    "Fast-CDR"
)
myiot_bootstrap_clone_if_missing(
    "${_myiot_fastdds_source_dir}"
    "https://github.com/eProsima/Fast-DDS.git"
    "${MYIOT_FASTDDS_VERSION}"
    "Fast-DDS"
)

set(_myiot_fastdds_common_args
    -DBUILD_TESTING:BOOL=OFF
)

myiot_bootstrap_configure_build_install(
    "foonathan_memory_vendor"
    "${_myiot_foonathan_source_dir}"
    "${MYIOT_BOOTSTRAP_DEPS_DIR}/foonathan_memory_vendor-build"
    EXTRA_CMAKE_ARGS
        ${_myiot_fastdds_common_args}
)
myiot_bootstrap_configure_build_install(
    "Fast-CDR"
    "${_myiot_fastcdr_source_dir}"
    "${MYIOT_BOOTSTRAP_DEPS_DIR}/Fast-CDR-build"
    EXTRA_CMAKE_ARGS
        ${_myiot_fastdds_common_args}
)
myiot_bootstrap_configure_build_install(
    "Fast-DDS"
    "${_myiot_fastdds_source_dir}"
    "${MYIOT_BOOTSTRAP_DEPS_DIR}/Fast-DDS-build"
    EXTRA_CMAKE_ARGS
        ${_myiot_fastdds_common_args}
        -DTHIRDPARTY:STRING=ON
        -DTHIRDPARTY_Asio:STRING=ON
        -DTHIRDPARTY_TinyXML2:STRING=ON
        -DTHIRDPARTY_UPDATE:BOOL=OFF
)
