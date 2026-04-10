#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${script_dir}/build"

cmake -S "${script_dir}" -B "${build_dir}"
cmake --build "${build_dir}"
cp "${build_dir}/sample_mail_sender" "${script_dir}/sample_mail_sender"

echo "built ${script_dir}/sample_mail_sender"
