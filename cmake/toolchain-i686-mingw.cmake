# Aeonian-OpenD2LOD - GPLv3
#
# Toolchain para gerar as DLLs que rodam DENTRO do Game.exe (PE32 / i686).
# Compila em Linux/macOS com mingw-w64, ou em Windows com o mesmo mingw.
#
# Uso:
#   cmake -S aeonian -B build/win32 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-i686-mingw.cmake
#
# Escolha do compilador (em ordem):
#   1. AEONIAN_MINGW_ROOT (variavel CMake ou de ambiente): prefixo de um
#      llvm-mingw (<root>/bin/i686-w64-mingw32-clang++).
#   2. i686-w64-mingw32-g++ no PATH (pacote gcc-mingw-w64-i686 / g++-mingw-w64-i686).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

if(NOT AEONIAN_MINGW_ROOT AND DEFINED ENV{AEONIAN_MINGW_ROOT})
    set(AEONIAN_MINGW_ROOT "$ENV{AEONIAN_MINGW_ROOT}")
endif()

set(_triple i686-w64-mingw32)
if(AEONIAN_MINGW_ROOT AND EXISTS "${AEONIAN_MINGW_ROOT}/bin/${_triple}-clang++")
    set(CMAKE_C_COMPILER   "${AEONIAN_MINGW_ROOT}/bin/${_triple}-clang")
    set(CMAKE_CXX_COMPILER "${AEONIAN_MINGW_ROOT}/bin/${_triple}-clang++")
    set(CMAKE_RC_COMPILER  "${AEONIAN_MINGW_ROOT}/bin/${_triple}-windres")
    set(CMAKE_FIND_ROOT_PATH "${AEONIAN_MINGW_ROOT}/${_triple}")
else()
    set(CMAKE_C_COMPILER   ${_triple}-gcc)
    set(CMAKE_CXX_COMPILER ${_triple}-g++)
    set(CMAKE_RC_COMPILER  ${_triple}-windres)
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
