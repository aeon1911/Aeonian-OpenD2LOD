# Aeonian-OpenD2LOD - GPLv3
#
# Ponto de entrada de mods para o Diablo II: Lord of Destruction 1.14d.
# Veja README.md e docs/SCOPE.md. O produto NAO e uma reimplementacao do jogo:
# e um conjunto de DLLs que rodam dentro do Game.exe original do usuario.
#
# Dois modos de build (detalhes em README.md):
#   HOST  (nativo)  : nucleo portavel + testes unitarios + ferramentas.
#   WIN32 (cross)   : DLLs i686 que rodam dentro do jogo
#                     (-DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-i686-mingw.cmake).

cmake_minimum_required(VERSION 3.21)
project(aeonian_opend2lod
    VERSION 0.1.0
    DESCRIPTION "Ponto de entrada de mods para Diablo II LoD 1.14d (GPLv3)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(AEONIAN_BUILD_TOOLS
    "Ferramentas de modding no host (extrator de MPQ; baixa o StormLib via rede)"
    OFF)

enable_testing()
add_subdirectory(aeonian)

if(AEONIAN_BUILD_TOOLS AND NOT WIN32)
    # StormLib (MIT, Ladislav Zezula): so o extrator de MPQ usa. Tag fixa e
    # dependencias bundled (zlib/bzip2), sem dependencia de sistema.
    include(FetchContent)
    set(STORM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(STORM_SKIP_INSTALL ON CACHE BOOL "" FORCE)
    set(STORM_USE_BUNDLED_LIBRARIES ON CACHE BOOL "" FORCE)
    FetchContent_Declare(stormlib
        GIT_REPOSITORY https://github.com/ladislav-zezula/StormLib.git
        GIT_TAG        v9.40
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(stormlib)
    add_subdirectory(tools)
endif()
