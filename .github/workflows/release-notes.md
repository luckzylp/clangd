Unstable snapshot of clangd on {{DATE}}.

Built from {{LLVM_REPO}}@{{LLVM_COMMIT}}.

## Build environment

Every target is built from the same LLVM revision and the same dependency
stack, so a binary can be reproduced from this table alone.

| Component | Version / configuration |
| --- | --- |
| LLVM / Clang / clang-tools-extra | `{{LLVM_REPO}}@{{LLVM_COMMIT}}`, `LLVM_ENABLE_PROJECTS=clang;clang-tools-extra` |
| ClangIR / MLIR | disabled (`CLANG_ENABLE_CIR=OFF`), not needed by clangd |
| gRPC | {{GRPC_VERSION}} (bundled Abseil LTS 20240722, protobuf 29.0.0, c-ares, re2, BoringSSL) |
| CMake | 3.31.6 on Linux and macOS (pinned; current LLVM requires >= 3.31); Windows uses the runner image's CMake, which satisfies the same requirement |
| Reported version | `clangd --version` prints the LLVM version of the commit above (development build, currently 24.0.0) |
| Build type | Release, `-DNDEBUG`, assertions off, no LTO |

| Target | Runner | Toolchain | C++ runtime | zlib | BoringSSL |
| --- | --- | --- | --- | --- | --- |
| linux-x86_64 | ubuntu-24.04, `ubuntu:20.04` container | clang 12 (glibc 2.31 headers) | static libstdc++ / libgcc | static `libz.a` | assembly enabled |
| linux-aarch64 | ubuntu-24.04-arm, `ubuntu:20.04` container | clang 12 (glibc 2.31 headers) | static libstdc++ / libgcc | static `libz.a` | assembly enabled |
| windows-x86_64 | windows-2022 | MSVC 19.4x, VS 2022 (v143) | static CRT (`/MT`) | disabled | assembly disabled |
| macos-arm64 | macos-15 | Apple clang (Xcode 16.4) | system libc++ | macOS SDK | assembly disabled |

## Compatibility

| Target | Minimum platform |
| --- | --- |
| linux-x86_64 | glibc 2.17 or newer: RHEL / CentOS 7, Debian 9, Ubuntu 16.04, and later releases |
| linux-aarch64 | glibc 2.17 or newer (aarch64 was introduced in glibc 2.17), e.g. RHEL / CentOS 7 aarch64 and later |
| windows-x86_64 | Windows 10 (1607) / Windows Server 2016 or newer, x64 |
| macos-arm64 | macOS 11.0 or newer on Apple Silicon |

The glibc floor is enforced by the build rather than documented after the fact:
`lib_compat.h` pins the libm symbols that glibc would otherwise version higher
to the architecture baseline (`GLIBC_2.2.5` on x86_64, `GLIBC_2.17` on
aarch64), libstdc++ and libgcc are linked statically, `libz.a` is forced static,
and `getentropy()` is provided locally because Abseil calls the glibc 2.25
symbol. `lib_compat_test.py --lib=GLIBC_2.18` rejects the build if anything
newer slips in; 2.18's only addition, `__cxa_thread_atexit_impl`, is a weak
reference, which is why a 2.18-whitelisted build still runs on 2.17 systems.

## Assets

| Asset | Contents |
| --- | --- |
| `clangd-<target>-<tag>.zip` | `bin/clangd`, builtin headers (`lib/clang/<version>/include`), licence |
| `clangd_indexing_tools-<target>-<tag>.zip` | `clangd-indexer`, `clangd-index-server`, builtin headers, licence |
| `clangd-debug-symbols-windows-x86_64-<tag>.7z` | PDB for the Windows build |

## Differences from the upstream `autobuild.yaml` pipeline

The original pipeline builds linux-x86_64, windows-x86_64 and one macOS binary
that covers both architectures, pinning a different gRPC release per platform
(v1.36.3 on Linux and Windows, v1.51.3 on macOS). This pipeline keeps the same
old-glibc approach for Linux, but differs in the following ways, all
intentional:

- **Different target set.** linux-aarch64 is added, and macOS is built natively
  for arm64 instead of as a universal x86_64 + arm64 binary. Windows arm64 was
  evaluated and dropped: the arm64 MSVC in the VS2026 runner image (14.52) hits
  known compiler defect C1001 on LLVM sources, which Microsoft fixed only in
  19.51. The macOS x86_64 slice is dropped as well, because `macos-15-intel`
  cannot complete this tree inside GitHub's six hour job limit.
- **One gRPC release for every target** ({{GRPC_VERSION}}) instead of one per
  platform. gRPC >= 1.68 is required for the abseil fix that makes the macOS
  arm64 build work, and the abseil bundled with 1.36 cannot be built by current
  toolchains at all. The consequence is that
  `clangd-index-server-monitor` is no longer shipped: it calls
  `Status::message().as_string()`, which stopped compiling against protobuf
  releases newer than the 3.15 that gRPC 1.36 bundled.
- **No MLIR in the build.** `CLANG_ENABLE_CIR` defaults to ON upstream, which
  silently appends MLIR as a dependency-only project and builds
  `mlir-tblgen` for a tool that clangd never uses. It is turned off here, which
  also removes the translation units that crashed the arm64 MSVC.
- **CMake 3.31.6 is pinned** on Linux and macOS. Current LLVM requires >= 3.31,
  while those images ship 3.16, 3.28 and 4.4 respectively.
- **The CMake step is no longer a folded scalar with `#` comments in it.** In
  the original workflow those comments are folded into the single command line,
  so the shell treats the rest of the line as a comment and silently drops
  `-DGRPC_INSTALL_PATH`, `-DLLVM_ENABLE_ZSTD=OFF` and the whole per-target
  option list.
- **The static MSVC runtime is actually enforced.** `gRPC_MSVC_STATIC_RUNTIME`
  only rewrites the flag strings, which Abseil and protobuf override in their
  own directories; `ABSL_MSVC_STATIC_RUNTIME` and
  `protobuf_MSVC_STATIC_RUNTIME` are therefore pinned from the command line,
  otherwise the link fails on `__imp_` symbols.
- **BoringSSL assembly is disabled where it requires extra tooling.** Windows
  builds would need NASM on PATH and macOS universal builds cannot use
  per-architecture assembly; Linux keeps the optimised assembly. Only the TLS
  throughput of the remote index client is affected.
- **macOS uses the SDK zlib and skips the runtimes.** gRPC's bundled zlib does
  not compile against the macOS 15 SDK, and compiler-rt on Darwin builds
  variants for macOS, iOS, tvOS and watchOS that clangd never uses. The macOS
  archive therefore contains the builtin headers only, no `libclang_rt`.
- **The compatibility gate runs before packaging**, not after the upload, so a
  binary that needs a newer glibc never becomes a published artifact.
- **Actions run on Node 24** (`checkout@v5`, `upload-artifact@v7`,
  `download-artifact@v8`); the original uses `checkout@v2` and
  `upload-artifact@v4`, whose runners are retired or deprecated.
