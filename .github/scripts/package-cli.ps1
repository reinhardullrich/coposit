param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$Platform,
    [Parameter(Mandatory = $true)][string]$Triplet,
    [Parameter(Mandatory = $true)][string]$VcpkgRoot
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$buildDir = Join-Path $repoRoot "cpp/build-release-$Platform"
$packageName = "coposit-$Version-$Platform"
$packageDir = Join-Path $repoRoot "dist/$packageName"
$archive = Join-Path $repoRoot "dist/$packageName.zip"
$tripletsDir = Join-Path $repoRoot ".github/triplets"

& (Join-Path $PSScriptRoot "install-vcpkg.ps1") -Triplet $Triplet -VcpkgRoot $VcpkgRoot

cmake -B $buildDir -S (Join-Path $repoRoot "cpp") `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
    "-DVCPKG_TARGET_TRIPLET=$Triplet" `
    "-DVCPKG_OVERLAY_TRIPLETS=$tripletsDir" `
    -DCOPOSIT_BUILD_APPS=ON `
    -DCOPOSIT_BUILD_PYTHON=OFF `
    -DCOPOSIT_BUILD_TESTS=OFF `
    -DCOPOSIT_BUILD_EXPERIMENTS=OFF
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

cmake --build $buildDir --config Release -j 4
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

$launcher = Join-Path $buildDir "Release/coposit.exe"
$companion = Join-Path $buildDir "Release/coposit-engine.exe"
$actualVersion = (& $launcher --version).Trim()
if ($actualVersion -ne $Version) {
    throw "Release version $Version does not match binary version $actualVersion"
}
$classification = (& $launcher --mode both "2#1,-1,1") -join "`n"
if ($classification -ne "copositive=true`nstrictly_copositive=false") {
    throw "Release classification smoke test failed"
}

New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
Copy-Item $launcher, $companion, (Join-Path $repoRoot "LICENSE"), (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md") $packageDir -Force
Compress-Archive -Path $packageDir -DestinationPath $archive -Force
