set(MYIOT_PETALINUX_PROJECT
    "/home/ph/work/proj/JNDM123"
    CACHE PATH "Path to the PetaLinux project used as the cross-compilation environment")

if(NOT EXISTS "${MYIOT_PETALINUX_PROJECT}/build/tmp")
    message(FATAL_ERROR
        "MYIOT_PETALINUX_PROJECT does not look like a built PetaLinux project: "
        "${MYIOT_PETALINUX_PROJECT}")
endif()

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_myiot_petalinux_tmp "${MYIOT_PETALINUX_PROJECT}/build/tmp")

function(_myiot_require_first_match out_var)
    unset(_myiot_matches)
    foreach(_pattern IN LISTS ARGN)
        file(GLOB _myiot_pattern_matches LIST_DIRECTORIES true "${_pattern}")
        if(_myiot_pattern_matches)
            list(SORT _myiot_pattern_matches)
            list(GET _myiot_pattern_matches 0 _myiot_first_match)
            set(${out_var} "${_myiot_first_match}" PARENT_SCOPE)
            return()
        endif()
    endforeach()

    string(REPLACE ";" "\n  " _myiot_patterns "${ARGN}")
    message(FATAL_ERROR "Could not locate any expected PetaLinux artifact. Checked:\n  ${_myiot_patterns}")
endfunction()

_myiot_require_first_match(MYIOT_PETALINUX_SYSROOT
    "${_myiot_petalinux_tmp}/work/x86_64-nativesdk-petalinux-linux/meta-environment-*/1.0-r*/recipe-sysroot"
)
_myiot_require_first_match(MYIOT_PETALINUX_NATIVE_SYSROOT
    "${_myiot_petalinux_tmp}/work/x86_64-nativesdk-petalinux-linux/meta-environment-*/1.0-r*/recipe-sysroot-native"
)
_myiot_require_first_match(MYIOT_PETALINUX_CC
    "${_myiot_petalinux_tmp}/work/x86_64-nativesdk-petalinux-linux/gcc-cross-canadian-arm/*/recipe-sysroot-native/usr/bin/arm-xilinx-linux-gnueabi/arm-xilinx-linux-gnueabi-gcc"
)
_myiot_require_first_match(MYIOT_PETALINUX_CXX
    "${_myiot_petalinux_tmp}/work/x86_64-nativesdk-petalinux-linux/gcc-cross-canadian-arm/*/recipe-sysroot-native/usr/bin/arm-xilinx-linux-gnueabi/arm-xilinx-linux-gnueabi-g++"
)
_myiot_require_first_match(MYIOT_QEMU_ARM
    "${_myiot_petalinux_tmp}/sysroots-components/x86_64/qemu-native/usr/bin/qemu-arm"
    "${_myiot_petalinux_tmp}/work/x86_64-nativesdk-petalinux-linux/nativesdk-qemu/*/image/opt/petalinux/2019.1/sysroots/x86_64-petalinux-linux/usr/bin/qemu-arm"
)

get_filename_component(MYIOT_PETALINUX_TOOLCHAIN_BINDIR "${MYIOT_PETALINUX_CC}" DIRECTORY)

set(_myiot_component_arch_dir "")
file(GLOB _myiot_component_arch_dirs LIST_DIRECTORIES true "${_myiot_petalinux_tmp}/sysroots-components/*")
list(SORT _myiot_component_arch_dirs)
foreach(_myiot_dir IN LISTS _myiot_component_arch_dirs)
    get_filename_component(_myiot_dir_name "${_myiot_dir}" NAME)
    if(IS_DIRECTORY "${_myiot_dir}" AND NOT _myiot_dir_name MATCHES "^(all|allarch|x86_64|x86_64-nativesdk)$")
        set(_myiot_component_arch_dir "${_myiot_dir}")
        break()
    endif()
endforeach()

if(NOT _myiot_component_arch_dir)
    message(FATAL_ERROR
        "Could not locate target sysroot components under ${_myiot_petalinux_tmp}/sysroots-components")
endif()

set(MYIOT_PETALINUX_COMPONENTS_DIR "${_myiot_component_arch_dir}" CACHE PATH "Target sysroot components directory")

set(_myiot_find_roots "${MYIOT_PETALINUX_SYSROOT}")
file(GLOB _myiot_component_roots LIST_DIRECTORIES true "${MYIOT_PETALINUX_COMPONENTS_DIR}/*")
list(SORT _myiot_component_roots)
foreach(_myiot_component_root IN LISTS _myiot_component_roots)
    if(IS_DIRECTORY "${_myiot_component_root}")
        list(APPEND _myiot_find_roots "${_myiot_component_root}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _myiot_find_roots)

set(_myiot_pkgconfig_dirs)
foreach(_myiot_root IN LISTS _myiot_find_roots)
    foreach(_myiot_pkgconfig_rel
        "usr/lib/pkgconfig"
        "usr/share/pkgconfig"
        "usr/lib/arm-xilinx-linux-gnueabi/pkgconfig"
    )
        if(EXISTS "${_myiot_root}/${_myiot_pkgconfig_rel}")
            list(APPEND _myiot_pkgconfig_dirs "${_myiot_root}/${_myiot_pkgconfig_rel}")
        endif()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES _myiot_pkgconfig_dirs)
list(JOIN _myiot_pkgconfig_dirs ":" _myiot_pkgconfig_path)

set(_myiot_cpu_flags "-march=armv7-a -mthumb -mfpu=neon -mfloat-abi=hard -mcpu=cortex-a9")
set(CMAKE_SYSROOT "${MYIOT_PETALINUX_SYSROOT}")
set(CMAKE_STAGING_PREFIX "${CMAKE_BINARY_DIR}/staging")
set(CMAKE_C_COMPILER "${MYIOT_PETALINUX_CC}")
set(CMAKE_CXX_COMPILER "${MYIOT_PETALINUX_CXX}")
set(CMAKE_ASM_COMPILER "${MYIOT_PETALINUX_CC}")
set(CMAKE_AR "${MYIOT_PETALINUX_TOOLCHAIN_BINDIR}/arm-xilinx-linux-gnueabi-ar")
set(CMAKE_NM "${MYIOT_PETALINUX_TOOLCHAIN_BINDIR}/arm-xilinx-linux-gnueabi-nm")
set(CMAKE_RANLIB "${MYIOT_PETALINUX_TOOLCHAIN_BINDIR}/arm-xilinx-linux-gnueabi-ranlib")
set(CMAKE_STRIP "${MYIOT_PETALINUX_TOOLCHAIN_BINDIR}/arm-xilinx-linux-gnueabi-strip")
set(CMAKE_OBJCOPY "${MYIOT_PETALINUX_TOOLCHAIN_BINDIR}/arm-xilinx-linux-gnueabi-objcopy")
set(CMAKE_OBJDUMP "${MYIOT_PETALINUX_TOOLCHAIN_BINDIR}/arm-xilinx-linux-gnueabi-objdump")

set(CMAKE_C_FLAGS_INIT "${_myiot_cpu_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_myiot_cpu_flags}")
set(CMAKE_ASM_FLAGS_INIT "${_myiot_cpu_flags}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed")

set(CMAKE_FIND_ROOT_PATH "${_myiot_find_roots}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(ENV{PKG_CONFIG_DIR} "")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${MYIOT_PETALINUX_SYSROOT}")
set(ENV{PKG_CONFIG_LIBDIR} "${_myiot_pkgconfig_path}")
set(ENV{PKG_CONFIG_PATH} "${_myiot_pkgconfig_path}")

if(EXISTS "${MYIOT_PETALINUX_NATIVE_SYSROOT}/usr/bin/pkg-config")
    set(PKG_CONFIG_EXECUTABLE
        "${MYIOT_PETALINUX_NATIVE_SYSROOT}/usr/bin/pkg-config"
        CACHE FILEPATH "pkg-config executable from the PetaLinux native sysroot" FORCE)
endif()

if(EXISTS "${MYIOT_PETALINUX_COMPONENTS_DIR}/openssl")
    set(OPENSSL_ROOT_DIR
        "${MYIOT_PETALINUX_COMPONENTS_DIR}/openssl"
        CACHE PATH "Target OpenSSL sysroot component from the PetaLinux build" FORCE)
    if(EXISTS "${OPENSSL_ROOT_DIR}/usr/include/openssl/ssl.h")
        set(OPENSSL_INCLUDE_DIR
            "${OPENSSL_ROOT_DIR}/usr/include"
            CACHE PATH "Target OpenSSL include directory from the PetaLinux build" FORCE)
    endif()
    if(EXISTS "${OPENSSL_ROOT_DIR}/usr/lib/libssl.so")
        set(OPENSSL_SSL_LIBRARY
            "${OPENSSL_ROOT_DIR}/usr/lib/libssl.so"
            CACHE FILEPATH "Target OpenSSL SSL library from the PetaLinux build" FORCE)
    endif()
    if(EXISTS "${OPENSSL_ROOT_DIR}/usr/lib/libcrypto.so")
        set(OPENSSL_CRYPTO_LIBRARY
            "${OPENSSL_ROOT_DIR}/usr/lib/libcrypto.so"
            CACHE FILEPATH "Target OpenSSL Crypto library from the PetaLinux build" FORCE)
    endif()
endif()

set(MYIOT_PETALINUX_SYSROOT "${MYIOT_PETALINUX_SYSROOT}" CACHE PATH "Resolved target sysroot")
set(MYIOT_PETALINUX_NATIVE_SYSROOT "${MYIOT_PETALINUX_NATIVE_SYSROOT}" CACHE PATH "Resolved native sysroot")
set(MYIOT_PETALINUX_TOOLCHAIN_BINDIR "${MYIOT_PETALINUX_TOOLCHAIN_BINDIR}" CACHE PATH "Resolved cross-toolchain bin directory")
set(MYIOT_QEMU_ARM "${MYIOT_QEMU_ARM}" CACHE FILEPATH "qemu-arm executable used to run target build tools on the host")
