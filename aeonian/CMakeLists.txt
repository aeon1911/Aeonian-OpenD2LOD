# Aeonian-OpenD2LOD - GPLv3
#
# Ponto de entrada de mods para o Diablo II: Lord of Destruction 1.14d.
#
# Dois modos de build:
#   * HOST (Linux/macOS/Windows nativo): so o nucleo portavel + testes.
#       cmake -S aeonian -B build/host && cmake --build build/host && \
#       ctest --test-dir build/host
#   * WIN32 (cross, i686 mingw): as DLLs que rodam dentro do Game.exe.
#       cmake -S aeonian -B build/win32 \
#             -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-i686-mingw.cmake
#       cmake --build build/win32
#
# Dependencias de terceiros: nenhuma (so a biblioteca padrao e a Win32).

cmake_minimum_required(VERSION 3.21)
project(aeonian
    VERSION 0.1.0
    DESCRIPTION "Aeonian: ponto de entrada de mods para Diablo II LoD 1.14d"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT TARGET ae_warnings)
    add_library(ae_warnings INTERFACE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ae_warnings INTERFACE
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor)
    elseif(MSVC)
        target_compile_options(ae_warnings INTERFACE /W4)
    endif()
endif()

# Nucleo portavel: INI, VFS, PE, manifesto, banco de enderecos, linha de
# comando, registro de faixas alteradas. Nenhuma chamada Win32.
add_library(ae_core STATIC
    core/address_db.cpp
    core/cmdline.cpp
    core/ini.cpp
    core/mod_manifest.cpp
    core/patch_registry.cpp
    core/pe_image.cpp
    core/vfs.cpp)
target_include_directories(ae_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ae_core PRIVATE ae_warnings)

if(WIN32)
    # Codigo que roda dentro do jogo: sem excecoes/RTTI e com a runtime
    # estatica, para nao depender de nada alem das DLLs do sistema.
    set(AE_WIN_COMPILE -fno-exceptions -fno-rtti)
    set(AE_WIN_LINK -static)

    # aeonian.dll - o motor de mods.
    add_library(aeonian SHARED
        host/dllmain.cpp
        host/file_index.cpp
        host/hooks.cpp
        host/inline_hook.cpp
        host/log.cpp
        host/media.cpp
        host/runtime.cpp
        host/video.cpp
        host/video_gl.cpp)
    target_link_libraries(aeonian PRIVATE ae_core ae_warnings gdi32 user32)
    target_include_directories(aeonian PRIVATE include)
    target_compile_definitions(aeonian PRIVATE
        AEONIAN_VERSION="${PROJECT_VERSION}")
    target_compile_options(aeonian PRIVATE ${AE_WIN_COMPILE})
    target_link_options(aeonian PRIVATE ${AE_WIN_LINK})
    set_target_properties(aeonian PROPERTIES PREFIX "")

    # ijl11.dll - proxy (ponto de entrada nao destrutivo).
    add_library(ijl11_proxy SHARED proxy/ijl11_proxy.cpp proxy/ijl11.def)
    target_link_libraries(ijl11_proxy PRIVATE ae_warnings)
    target_compile_options(ijl11_proxy PRIVATE ${AE_WIN_COMPILE})
    target_link_options(ijl11_proxy PRIVATE ${AE_WIN_LINK})
    set_target_properties(ijl11_proxy PROPERTIES PREFIX "" OUTPUT_NAME "ijl11")

    # Plugin de exemplo para modders.
    add_library(hello_plugin SHARED examples/hello-plugin/hello.cpp)
    target_include_directories(hello_plugin PRIVATE include)
    target_link_libraries(hello_plugin PRIVATE ae_warnings user32)
    target_compile_options(hello_plugin PRIVATE ${AE_WIN_COMPILE})
    target_link_options(hello_plugin PRIVATE ${AE_WIN_LINK})
    set_target_properties(hello_plugin PROPERTIES PREFIX "" OUTPUT_NAME "hello")

    # Plugin de exemplo de HUD (on_present).
    add_library(hud_demo_plugin SHARED examples/hud-demo/hud.cpp)
    target_include_directories(hud_demo_plugin PRIVATE include)
    target_link_libraries(hud_demo_plugin PRIVATE ae_warnings gdi32 user32)
    target_compile_options(hud_demo_plugin PRIVATE ${AE_WIN_COMPILE})
    target_link_options(hud_demo_plugin PRIVATE ${AE_WIN_LINK})
    set_target_properties(hud_demo_plugin PROPERTIES PREFIX "" OUTPUT_NAME "hud")

    # Mod de mapa (C1/C2): marca monstros, objetivos e saidas no automap.
    add_library(map_overlay_plugin SHARED
        examples/map-overlay/map_overlay.cpp
        examples/map-overlay/map_logic.cpp)
    target_include_directories(map_overlay_plugin PRIVATE include)
    target_link_libraries(map_overlay_plugin PRIVATE ae_core ae_warnings gdi32 user32)
    target_compile_options(map_overlay_plugin PRIVATE ${AE_WIN_COMPILE})
    target_link_options(map_overlay_plugin PRIVATE ${AE_WIN_LINK})
    set_target_properties(map_overlay_plugin PROPERTIES
        PREFIX "" OUTPUT_NAME "map_overlay")

    # Plugin de autoteste (so para tests/integration).
    add_library(selftest_plugin SHARED
        tests/integration/selftest-plugin/selftest.cpp)
    target_include_directories(selftest_plugin PRIVATE include)
    target_link_libraries(selftest_plugin PRIVATE ae_warnings gdi32 user32)
    target_compile_options(selftest_plugin PRIVATE ${AE_WIN_COMPILE})
    target_link_options(selftest_plugin PRIVATE ${AE_WIN_LINK})
    set_target_properties(selftest_plugin PROPERTIES
        PREFIX "" OUTPUT_NAME "selftest")

    # Condutor de teste do mod de mapa (so para tests/integration): leva o
    # jogo ate uma partida e grava quadros.
    add_library(mapdrive_plugin SHARED
        tests/integration/mapdrive-plugin/mapdrive.cpp
        examples/map-overlay/map_logic.cpp)
    target_include_directories(mapdrive_plugin PRIVATE include examples/map-overlay)
    target_link_libraries(mapdrive_plugin PRIVATE ae_warnings user32)
    target_compile_options(mapdrive_plugin PRIVATE ${AE_WIN_COMPILE})
    target_link_options(mapdrive_plugin PRIVATE ${AE_WIN_LINK})
    set_target_properties(mapdrive_plugin PROPERTIES
        PREFIX "" OUTPUT_NAME "mapdrive")
else()
    enable_testing()
    add_executable(ae_core_test tests/core_test.cpp)
    target_link_libraries(ae_core_test PRIVATE ae_core ae_warnings)
    target_compile_definitions(ae_core_test PRIVATE
        AEONIAN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME core COMMAND ae_core_test)

    # Logica portavel do mod map-overlay (formula do automap, classificacao,
    # varredura das estruturas) sobre uma memoria sintetica.
    add_executable(ae_map_overlay_test
        examples/map-overlay/map_logic_test.cpp
        examples/map-overlay/map_logic.cpp)
    target_link_libraries(ae_map_overlay_test PRIVATE ae_core ae_warnings)
    target_compile_definitions(ae_map_overlay_test PRIVATE
        AEONIAN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME map_overlay COMMAND ae_map_overlay_test)
endif()
