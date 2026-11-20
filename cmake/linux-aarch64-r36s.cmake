# ARM64 Linux handheld build (R36S, RK3326: Cortex-A35, Mali-G31) cross-compiled on a
# 64-bit host against an old-glibc sysroot (Ubuntu 20.04 arm64, glibc 2.31) so
# the binary runs on ArkOS. Set R36S_SYSROOT to the sysroot's directory.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(_sysroot $ENV{R36S_SYSROOT})
set(CMAKE_SYSROOT ${_sysroot})
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_COMPILER_TARGET aarch64-linux-gnu)
set(CMAKE_CXX_COMPILER_TARGET aarch64-linux-gnu)
set(_flags "-mcpu=cortex-a35 --gcc-install-dir=${_sysroot}/usr/lib/gcc/aarch64-linux-gnu/10")
set(CMAKE_C_FLAGS_INIT "${_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_flags}")
# The device has no libstdc++ 10 to match: link it in.
set(CMAKE_EXE_LINKER_FLAGS_INIT "--ld-path=/usr/bin/aarch64-linux-gnu-ld -static-libstdc++ -static-libgcc")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "--ld-path=/usr/bin/aarch64-linux-gnu-ld -static-libstdc++ -static-libgcc")
set(CMAKE_FIND_ROOT_PATH ${_sysroot})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
