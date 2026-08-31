param(
    [Parameter(Mandatory = $true)][string]$Triplet,
    [Parameter(Mandatory = $true)][string]$VcpkgRoot
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$cacheDir = Join-Path $repoRoot ".vcpkg-cache"
$vcpkgTag = "2026.07.29"

New-Item -ItemType Directory -Force -Path $cacheDir | Out-Null
if (-not (Test-Path (Join-Path $VcpkgRoot ".git"))) {
    git clone --branch $vcpkgTag --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgRoot
    if ($LASTEXITCODE -ne 0) { throw "vcpkg clone failed" }
}

# vcpkg 2026.07.29's GMP port names an MSYS2 archive that was removed from every official mirror.
$gmpPort = Join-Path $VcpkgRoot "ports/gmp/portfile.cmake"
$gmpPortText = [IO.File]::ReadAllText($gmpPort)
$gmpPortText = $gmpPortText.Replace(
    "autoconf2.71-2.71-3-any.pkg.tar.zst",
    "autoconf2.71-2.71-4-any.pkg.tar.zst"
).Replace(
    "dd312c428b2e19afd00899eb53ea4255794dea4c19d1d6dea2419cb6a54209ea2130d48abbc20af12196b9f628143436f736fbf889809c2c2291be0c69c0e306",
    "c93b791eb55893cbe7c425e764074837355fd165deb7b1775f652c8e25d9d1f0cdd4120ab710d56fb859b7df55c4f971eccda7c112448f60615bff8a2dc81166"
)
[IO.File]::WriteAllText($gmpPort, $gmpPortText)

& (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
if ($LASTEXITCODE -ne 0) { throw "vcpkg bootstrap failed" }

$env:VCPKG_BINARY_SOURCES = "clear;files,$cacheDir,readwrite"
& (Join-Path $VcpkgRoot "vcpkg.exe") install "flint:$Triplet" "--overlay-triplets=$repoRoot/.github/triplets"
if ($LASTEXITCODE -ne 0) { throw "vcpkg dependency installation failed" }
