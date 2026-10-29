# Builds and releases

The [Build and release workflow](https://github.com/Lost-Engines/tfx-redux/actions/workflows/build.yml)
follows the Darker projects: Debug and Release builds on Linux, Windows and
macOS, asset-independent CTest checks, Runtime-only archives, and a generated
release after all six jobs pass. Builds use C++26, GCC 16 on Linux/macOS, and
MSYS2 UCRT64 GCC on Windows. The compiler cache is separate for each platform
and configuration.

All three packages target x86-64. TFX's faithful flight calculations use x87
extended precision and instructions; a native Apple Silicon build cannot yet
run that backend. The macOS package is built on Intel, for macOS 15 or newer.
The Linux package is built on Ubuntu 24.04. No game installation, firmware,
private research exports or screenshot gallery is needed for these CI tests.

## When a release is published

Relevant pushes to `master` and pull requests run the build matrix. Changes to
`src/`, `vectorstorm/`, `CMakeLists.txt` or `cmake/` (except the archive wrapper)
also publish a release after the tests pass. Test, helper-script and workflow
changes run the checks without making another release. Documentation and
gallery-only changes do not trigger the game workflow. A manual run has a
**release** checkbox (off by default); publication is restricted to `master`.

Each release has a `release-<full commit SHA>` tag, a package per platform and
`SHA256SUMS`. A new release is uploaded as a draft, populated with all three
archives and checksums, then published. Existing releases and tags are retained.
An older build cannot replace a newer release as the latest download.

The archives contain `tfx` (or `tfx.exe`), the relevant fetch helper and its
manifests, setup/control documentation, and licence notices (including the fetched libraries’ licences). Runtime packages
exclude game data, CD images, ROMs, saves, capture images and build/test outputs.
GLFW is built from the pinned source and linked statically, so an installed
GLFW shared library cannot accidentally become a package dependency. Compiler
runtimes are linked statically; Windows checks the installed executable
with only system directories in `PATH`. Original game data and ROMs are fetched
separately using the supplied helper.

## Reproduce locally

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=OFF -DTFX_STATIC_RUNTIME=ON \
  -DCMAKE_DISABLE_FIND_PACKAGE_glfw3=ON
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
cmake --install build-release --prefix "$PWD/build-release/package/tfx-redux" --component Runtime
cmake -DPACKAGE_ROOT="$PWD/build-release/package" \
  -DOUTPUT_DIR="$PWD/build-release/dist" -DPLATFORM=linux-x86_64 -P cmake/package.cmake
```

Use `windows-x86_64` or `macos-x86_64` for the corresponding host build.
The CI artifacts include Debug packages and test reports for diagnosis;
only Release packages are attached to the public release.
