#!/usr/bin/env bash

set -euo pipefail

image="${MYIOT_NODE_DOCKER_IMAGE:-myiot-node24:local}"
workspace="${PWD}"
cache_dir="${MYIOT_NPM_CACHE_DIR:-/tmp/myiot-npm-cache}"

mkdir -p "${cache_dir}"

exec docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "${workspace}:${workspace}" \
    --volume "${cache_dir}:/tmp/npm-cache" \
    --workdir "${workspace}" \
    --env HOME=/tmp \
    --env npm_config_cache=/tmp/npm-cache \
    "${image}" \
    npm "$@"
