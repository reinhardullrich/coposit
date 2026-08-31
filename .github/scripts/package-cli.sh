#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 4 ]]; then
    echo "Usage: $0 VERSION PLATFORM TRIPLET VCPKG_ROOT" >&2
    exit 2
fi

version="$1"
platform="$2"
triplet="$3"
vcpkg_root="$4"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
build_dir="$repo_root/cpp/build-release-$platform"
package_name="coposit-$version-$platform"
package_dir="$repo_root/dist/$package_name"
incumbent="$(tr -d '\r\n' < "$repo_root/python/pycoposit/incumbent_model.txt")"

"$script_dir/install-vcpkg.sh" "$triplet" "$vcpkg_root"

linker_flags=""
if [[ "$platform" == linux-* ]]; then
    linker_flags="-static-libgcc -static-libstdc++"
fi

cmake -B "$build_dir" -S "$repo_root/cpp" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$vcpkg_root/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="$triplet" \
    -DVCPKG_OVERLAY_TRIPLETS="$repo_root/.github/triplets" \
    -DCOPOSIT_BUILD_APPS=ON \
    -DCOPOSIT_BUILD_PYTHON=OFF \
    -DCOPOSIT_BUILD_TESTS=OFF \
    -DCOPOSIT_BUILD_EXPERIMENTS=OFF \
    -DCMAKE_EXE_LINKER_FLAGS_RELEASE="$linker_flags"
cmake --build "$build_dir" --config Release -j 4

launcher="$build_dir/coposit"
companion="$build_dir/coposit-$incumbent"
actual_version="$("$launcher" --version)"
if [[ "$actual_version" != "$version" ]]; then
    echo "Release version $version does not match binary version $actual_version" >&2
    exit 1
fi
if [[ "$("$launcher" --mode both '2#1,-1,1')" != $'copositive=true\nstrictly_copositive=false' ]]; then
    echo "Release classification smoke test failed" >&2
    exit 1
fi

strip "$launcher" "$companion"
mkdir -p "$package_dir"
cp "$launcher" "$companion" "$repo_root/LICENSE" "$repo_root/THIRD_PARTY_NOTICES.md" "$package_dir/"
tar -C "$repo_root/dist" -czf "$repo_root/dist/$package_name.tar.gz" "$package_name"
