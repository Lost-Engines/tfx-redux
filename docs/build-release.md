# Builds and releases

The [Build and release workflow](https://github.com/Lost-Engines/tfx-redux/actions/workflows/build.yml)
follows the Darker projects: Debug and Release builds on Linux, Windows and
macOS, asset-independent CTest checks, Runtime-only archives, and a generated
release after the build matrix and asset-helper checks pass. Builds use C++26, GCC 16 on Linux/macOS, and
MSYS2 UCRT64 GCC on Windows. The compiler cache is separate for each platform
and configuration.

Linux and Windows packages target x86-64. macOS has separate Intel and Apple
Silicon packages, built on the corresponding hardware for macOS 15 or newer.
Redux uses portable float/double arithmetic throughout its own code; see
[the numerical checks](portable-arithmetic.md).
CI uses `ubuntu-latest`, `windows-latest` and `macos-latest` (Apple Silicon)
so new runner images are tested as GitHub rolls them out. Intel Mac builds
use `macos-26-intel`; GitHub does not provide a standard Intel `-latest` label.
See the [available runner labels](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
No game installation, firmware, private research exports or screenshot gallery
is needed for these CI tests.

## When a release is published

Relevant pushes to `master` and pull requests run the build matrix. Changes to
`src/`, `vectorstorm/`, `CMakeLists.txt`, packaged `scripts/`, or `cmake/` (except the archive wrapper)
also publish a release after the tests pass. Test, development-tool and workflow
changes run the checks without making another release. Documentation and
gallery-only changes do not trigger the game workflow. A manual run has a
**release** checkbox (off by default); publication is restricted to `master`.

Each release has a `release-<full commit SHA>` tag, a package per platform and
`SHA256SUMS`. A new release is uploaded as a draft, populated with all four
archives and checksums, then published. Superseded published releases and
generated release tags are removed after their replacement is complete.
An older build cannot replace a newer release as the latest download.

The archives contain `tfx` (or `tfx.exe`), the relevant fetch helper and its
manifests, setup/control documentation, and licence notices (including the fetched libraries’ licences). Runtime packages
exclude game data, CD images, ROMs, saves, capture images and build/test outputs.
GLFW is built from upstream `master` and linked statically, so an installed
GLFW shared library cannot accidentally become a package dependency. Compiler
runtimes are linked statically; Windows checks the installed executable
with only system directories in `PATH`. Original game data and ROMs are fetched
separately using the supplied helper.

## Reproduce locally

```sh
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON -DBUILD_SHARED_LIBS=OFF -DTFX_STATIC_RUNTIME=ON
cmake --build build-release -j
ctest --test-dir build-release --output-on-failure
cmake --install build-release --prefix "$PWD/build-release/package/tfx-redux" --component Runtime
cmake -DPACKAGE_ROOT="$PWD/build-release/package" \
  -DOUTPUT_DIR="$PWD/build-release/dist" -DPLATFORM=linux-x86_64 -P cmake/package.cmake
```

Use `windows-x86_64`, `macos-x86_64` or `macos-arm64` for the corresponding host build.
The CI artifacts include Debug packages and test reports for diagnosis;
only Release packages are attached to the public release.
