# Optional cross-build from Linux/macOS with the official llvm-mingw bundle.
# cmake -S . -B build/windows -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-mingw.cmake \
#       -DLLVM_MINGW_ROOT=/absolute/path/to/llvm-mingw
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(LLVM_MINGW_ROOT "$ENV{LLVM_MINGW_ROOT}" CACHE PATH "Extracted llvm-mingw toolchain")
if(NOT LLVM_MINGW_ROOT)
  message(FATAL_ERROR "Set LLVM_MINGW_ROOT to an extracted llvm-mingw directory")
endif()
set(CMAKE_C_COMPILER "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-clang++")
set(CMAKE_RC_COMPILER "${LLVM_MINGW_ROOT}/bin/x86_64-w64-mingw32-windres")
set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# Carry the selected toolchain through CMake compiler/ABI try_compile checks.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES LLVM_MINGW_ROOT)
