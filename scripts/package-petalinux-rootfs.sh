#!/usr/bin/env bash

set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd "${script_dir}/.." && pwd)

default_build_dir() {
    local webui_dir="${repo_root}/build-petalinux-jndm123-webui"
    local legacy_dir="${repo_root}/build-petalinux-jndm123"

    if [[ -d "${webui_dir}/staging/bin" ]]; then
        printf '%s\n' "${webui_dir}"
    else
        printf '%s\n' "${legacy_dir}"
    fi
}

petalinux_project=${MYIOT_PETALINUX_PROJECT:-/home/ph/work/proj/JNDM123}
build_dir=${MYIOT_BUILD_DIR:-$(default_build_dir)}
runtime_dir=${MYIOT_RUNTIME_DIR:-"${build_dir}/staging"}
recipe_dir="${petalinux_project}/project-spec/meta-user/recipes-apps/myiot/files"
package_file="${recipe_dir}/myiot-runtime.tar.gz"

if [[ ! -d "${runtime_dir}/bin" ]]; then
    echo "Runtime bin directory not found: ${runtime_dir}/bin" >&2
    exit 1
fi

if [[ ! -f "${runtime_dir}/bin/macchina" ]]; then
    echo "Runtime executable not found: ${runtime_dir}/bin/macchina" >&2
    exit 1
fi

if [[ ! -f "${runtime_dir}/bin/macchina.properties" ]]; then
    echo "Runtime config not found: ${runtime_dir}/bin/macchina.properties" >&2
    exit 1
fi

if [[ ! -d "${runtime_dir}/lib/bundles" ]]; then
    echo "Runtime bundle directory not found: ${runtime_dir}/lib/bundles" >&2
    exit 1
fi

mkdir -p "${recipe_dir}"

if [[ -f "${package_file}" ]]; then
    backup_file="${package_file}.bak.$(date +%Y%m%d%H%M%S)"
    cp -a "${package_file}" "${backup_file}"
    echo "Backup: ${backup_file}"
fi

tmp_package="${package_file}.tmp"
rm -f "${tmp_package}"

tmp_root=$(mktemp -d)
trap 'rm -rf "${tmp_root}"' EXIT

mkdir -p "${tmp_root}/bin" "${tmp_root}/lib/bundles"
cp -a "${runtime_dir}/bin/." "${tmp_root}/bin/"
find "${runtime_dir}/lib" -maxdepth 1 \( -type f -o -type l \) -name '*.so*' -exec cp -a {} "${tmp_root}/lib/" \;
cp -a "${runtime_dir}/lib/bundles/." "${tmp_root}/lib/bundles/"

tar -C "${tmp_root}" \
    --owner=0 \
    --group=0 \
    --numeric-owner \
    -czf "${tmp_package}" \
    bin \
    lib

mv "${tmp_package}" "${package_file}"

echo "Runtime directory: ${runtime_dir}"
echo "Updated rootfs archive: ${package_file}"
