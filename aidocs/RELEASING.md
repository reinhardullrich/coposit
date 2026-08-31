# Releasing coposit

Ordinary pushes and pull requests run no release builds. A release starts only when the `Release` workflow is manually run from
`main`.

1. Set the calendar version in `cpp/CMakeLists.txt` using `YYYY.M.D.N`, where `N` starts at 1 and increments for additional releases on the same day.
2. Commit and push the release-ready source.
3. In GitHub Actions, open `Release`, choose `Run workflow`, select `main`, and start it.

The workflow refuses to run from another branch or to reuse an existing version tag. It builds and checks:

- self-contained CLI packages for Linux x86-64, Linux ARM64, macOS Intel, macOS Apple Silicon, and Windows x86-64;
- the public launcher, its generically named private engine, and a known CP/SCP boundary classification on every CLI platform;
- `pycoposit` wheels for CPython 3.11, 3.12, 3.13, and 3.14 on those five platforms; and
- one Python source distribution containing the public incumbent implementation and the infrastructure needed to build it.

Only after every build succeeds does the workflow create the matching tag and GitHub release. It attaches the five CLI packages,
then publishes the wheels and Python source distribution to PyPI through the `pypi` GitHub environment and PyPI trusted publishing.
No API token is stored. Linux artifacts target glibc 2.28 or newer.

Unlike FracESSA's single-file CLI, coposit deliberately keeps its launcher separate from the private one-model engine that it
supervises. Each platform asset is therefore one archive containing both executables, `LICENSE`, and `THIRD_PARTY_NOTICES.md`:

```text
coposit-<version>-macos-apple-silicon-arm64.tar.gz
coposit-<version>-macos-intel-64bit.tar.gz
coposit-<version>-linux-intel-amd-64bit.tar.gz
coposit-<version>-linux-arm64.tar.gz
coposit-<version>-windows-intel-amd-64bit.zip
```

After extraction, invoke only `coposit`; `coposit-engine` is a private runtime dependency whose name does not expose the selected
production model. Python wheel filenames retain their standardized platform tags.

Release jobs run from `main`, so their vcpkg binary caches are shared by later releases. GMP, MPFR, and FLINT rebuild only when
their versions or release triplets change.

coposit and the complete statically linked release are distributed under GPL-3.0-or-later. The release page links the license,
tagged corresponding source, and `THIRD_PARTY_NOTICES.md`. Python wheels retain their packaged license files.
