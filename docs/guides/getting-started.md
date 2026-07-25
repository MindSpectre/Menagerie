# Getting Started

Menagerie is developed and tested on Linux; the `debug`, `release`, `dev-slim`, `asan`,
`tsan`, and coverage presets all target a clang + libc++ toolchain, linked with mold, with
ccache fronting the compiler. macOS and Windows presets exist in `CMakePresets.json` (MSVC
and clang-cl) but are experimental and not part of the regularly verified path. The rest of
this guide assumes Linux.

## Prerequisites

The toolchain observed during this guide's verification build (Fedora 44):

- clang / clang++ 22.1.8, with libc++ and libc++abi development headers
- CMake 4.3.0 (`cmakeMinimumRequired` in `CMakePresets.json` is 3.29)
- Ninja 1.13.2 (the generator every preset uses)
- ccache 4.12.3 (wired in as `CMAKE_CXX_COMPILER_LAUNCHER`/`CMAKE_C_COMPILER_LAUNCHER`)
- mold 2.40.4 (`CMAKE_EXE_LINKER_FLAGS`/`CMAKE_SHARED_LINKER_FLAGS` pass `-fuse-ld=mold`)
- git, curl, pkg-config, autoconf/automake/libtool, bison, flex, perl with `IPC::Cmd` (vcpkg
  port builds need these)
- OpenSSL development headers (vcpkg still builds its own OpenSSL port for the tree, but the
  system headers are a build-tool dependency of some ports)

`scripts/install-linux.sh` installs the rest of this set for Fedora/RHEL (dnf), Debian/Ubuntu
(apt), and Arch (pacman); `scripts/install-macos.sh` covers the experimental macOS path via
Homebrew. Run the one matching your distribution before configuring the project. None of the
three branches install the libc++/libc++abi development headers that `-stdlib=libc++` needs
(the toolchain Docker image adds them separately) - install those yourself first:

- Fedora/RHEL (dnf): `libcxx-devel libcxxabi-devel`
- Debian/Ubuntu (apt): `libc++-dev libc++abi-dev`
- Arch (pacman): `libc++ libc++abi`

## vcpkg setup

Dependencies (Boost, OpenSSL, jsoncpp, GoogleTest, libpq, nghttp2/ngtcp2/nghttp3, and the
benchmark-only ports) are declared in `vcpkg.json` and resolved through a vcpkg checkout
that lives inside the source tree. `vcpkg/` is gitignored - every clone provisions its own
copy, there is no submodule. `CMakePresets.json` points `CMAKE_TOOLCHAIN_FILE` at
`vcpkg/scripts/buildsystems/vcpkg.cmake` relative to the source directory, so vcpkg must be
cloned into the repository root under exactly that name:

```bash
git clone https://github.com/microsoft/vcpkg vcpkg
./vcpkg/bootstrap-vcpkg.sh
```

No specific vcpkg commit is pinned in the manifest (no `vcpkg-configuration.json` baseline);
the CI toolchain image (`infrastructure/toolchain/Dockerfile`) clones vcpkg the same way, so
tracking upstream's default branch matches what the tree is built against there. The first
configure after bootstrapping builds every port from source and is slow (several minutes);
subsequent configures reuse the installed tree under `build/<preset>/vcpkg_installed/`.

## Configure and build

| Preset | Use it for |
| --- | --- |
| `debug` | Default day-to-day build: full component set, tests, benchmarks, examples |
| `release` | Optimized build; what CI runs for unit and integration testing |
| `dev-slim` | Fast iteration on `common/` only - components and benchmarks disabled |
| `asan` | Debug build instrumented with AddressSanitizer + UndefinedBehaviorSanitizer |
| `tsan` | Debug build instrumented with ThreadSanitizer; `common/` only, no benchmarks |
| `llvm-coverage` | Debug build with clang source-based coverage (`llvm-profdata`/`llvm-cov`) |
| `gcc-coverage` | Debug build with gcov-style coverage (matches CLion's CTest coverage view) |
| `release-lto` | Release with link-time optimization |
| `release-perf` | Release codegen with frame pointers kept, for `perf record -g` profiling |
| `release-instrprof` | Release build instrumented for LLVM profiling-guided hotspot analysis |

Each preset has a matching build and test preset of the same name and configures into
`build/<preset>/`. The canonical sequence for the default `debug` preset:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Substitute the preset name to use a different configuration, for example
`cmake --preset dev-slim && cmake --build --preset dev-slim && ctest --preset dev-slim` for
a faster common/-only loop. `ctest --preset <name> -L unit` or `-L integration` restricts
the run to one label; integration tests that need PostgreSQL expect `POSTGRES_HOST`,
`POSTGRES_PORT`, `POSTGRES_DB`, `POSTGRES_USER`, `POSTGRES_PASSWORD` environment variables
pointing at a running server (see the PR workflow in `.github/workflows/pr-testing.yaml`
for the service container it starts).

## Options

Feature toggles live in `cmake/features.cmake` and are set with `-D<NAME>=ON|OFF` on the
`cmake --preset` invocation, or overridden per preset in `CMakePresets.json`:

- `USE_BOOST` - enable Boost (ON by default; most of `common/` and all of `components/http`
  depend on it)
- `BUILD_COMPONENTS` - build `components/` (http, database); OFF shrinks the build to
  `common/` only, as `dev-slim` and `tsan` do
- `USE_TESTS` - build the GoogleTest suites under `tests/`
- `DO_BENCHMARKS` - build the benchmark binaries under `benchmarks/`
- `BUILD_EXAMPLES` - build the example apps under `examples/`
- `ENABLE_LOGGING` - compile end-user logging in; also gates `COMPONENT_LOGGING`
- `COMPONENT_LOGGING` - logging inside components (only offered when `ENABLE_LOGGING` is ON)
- `KEEP_FRAME_POINTERS` - keep frame pointers in Release builds so `perf` call graphs
  resolve; off by default because Release otherwise omits them
- `BUILD_DOCS` - generate the Doxygen API site (target `docs`); OFF by default so a normal
  build does not require Doxygen
- `UNRECOVERABLE_EXCEPTIONS_TERMINATE` - `UNRECOVERABLE_NOEXCEPT` expands to `noexcept`,
  so unrecoverable exceptions such as `bad_alloc` terminate instead of unwinding
- `BUILD_HTTP`, `BUILD_DATABASE` - offered only when `BUILD_COMPONENTS` is ON; individually
  toggle the http and database components
- `BUILD_POSTGRESQL` - offered only when `BUILD_DATABASE` is ON; builds the PostgreSQL
  provider

## Building the docs

The Doxygen site is behind `BUILD_DOCS`, off by default so ordinary builds do not need
Doxygen installed:

```bash
cmake --preset debug -DBUILD_DOCS=ON
cmake --build build/debug --target docs
```

The generated site lands at `build/docs/html/index.html`; open it in a browser. The
`Doxyfile` (`docs/doxygen/Doxyfile`) indexes `README.md`, `docs/architecture`, `docs/guides`,
`common`, and `components`, and uses the vendored doxygen-awesome-css theme, so
`docs/architecture/*.md` and `docs/guides/*.md` - including `docs/guides/http-server.md` for
a walkthrough of running the example HTTP server - appear as pages alongside the generated
API reference.
