#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 2 ]]; then
    echo "Usage: $0 TRIPLET VCPKG_ROOT" >&2
    exit 2
fi

triplet="$1"
vcpkg_root="$2"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
cache_dir="$repo_root/.vcpkg-cache"
vcpkg_tag="2026.07.29"

case "$(uname -s)" in
    Linux)
        if [[ "$EUID" -eq 0 ]]; then
            dnf install -y autoconf autoconf-archive automake curl git libtool tar unzip zip
        fi
        ;;
    Darwin)
        brew install autoconf autoconf-archive automake libtool
        ;;
esac

mkdir -p "$cache_dir"
if [[ ! -d "$vcpkg_root/.git" ]]; then
    git clone --branch "$vcpkg_tag" --depth 1 https://github.com/microsoft/vcpkg.git "$vcpkg_root"
fi

"$vcpkg_root/bootstrap-vcpkg.sh" -disableMetrics
export VCPKG_BINARY_SOURCES="clear;files,$cache_dir,readwrite"
"$vcpkg_root/vcpkg" install "flint:$triplet" --overlay-triplets="$repo_root/.github/triplets"
