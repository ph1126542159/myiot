#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/.." && pwd)

default_build_dir() {
    local webui_dir="${repo_root}/build-petalinux-jndm123-webui"
    local legacy_dir="${repo_root}/build-petalinux-jndm123"

    if command -v npm >/dev/null 2>&1; then
        printf '%s\n' "${webui_dir}"
    else
        printf '%s\n' "${legacy_dir}"
    fi
}

petalinux_project=${MYIOT_PETALINUX_PROJECT:-/home/ph/work/proj/JNDM123}
build_dir=${MYIOT_BUILD_DIR:-$(default_build_dir)}
build_type=${CMAKE_BUILD_TYPE:-Release}
parallel_jobs=${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc)}

if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
    generator=${CMAKE_GENERATOR}
elif command -v ninja >/dev/null 2>&1; then
    generator=Ninja
else
    generator="Unix Makefiles"
fi

cmake_args=(
    -S "${repo_root}"
    -B "${build_dir}"
    -G "${generator}"
    -DCMAKE_BUILD_TYPE=${build_type}
    -DCMAKE_TOOLCHAIN_FILE=${repo_root}/cmake/toolchains/petalinux-jndm123.cmake
    -DMYIOT_PETALINUX_PROJECT=${petalinux_project}
)

if ! command -v npm >/dev/null 2>&1; then
    echo "npm was not found in PATH; building without Web UI bundles." >&2
    cmake_args+=(-DMYIOT_ENABLE_WEBUI=OFF)
fi

if (($# > 0)); then
    cmake_args+=("$@")
fi

cmake "${cmake_args[@]}"
cmake --build "${build_dir}" --parallel "${parallel_jobs}"
cmake --install "${build_dir}"

package_file="${build_dir}/myiot-petalinux-install.tar.gz"
tar -C "${build_dir}/install" -czf "${package_file}" .

echo "Install tree: ${build_dir}/install"
echo "Deploy archive: ${package_file}"
