// Aeonian-OpenD2LOD
// Copyright (C) 2026 Aeonian-OpenD2LOD contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// aeonian/host/dllmain.cpp
//
// aeonian.dll: o motor de mods. Quem o carrega (o proxy ijl11.dll, ou
// qualquer outro metodo de injecao, como um import extra no PE) chama
// `AeonianBootstrap` uma vez. Nada acontece no DllMain alem de guardar o
// modulo, porque ele roda sob o loader lock.

#include <windows.h>

#include "host/runtime.h"

#ifndef AEONIAN_VERSION
#define AEONIAN_VERSION "dev"
#endif

namespace {
HMODULE g_self = nullptr;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_self = instance;
    DisableThreadLibraryCalls(instance);
  }
  return TRUE;
}

extern "C" {

// Instala o Aeonian no processo atual. Idempotente. Retorna 0.
__declspec(dllexport) int AeonianBootstrap(void) {
  aeonian::Runtime::Get().Bootstrap(g_self);
  return 0;
}

__declspec(dllexport) const char* AeonianVersion(void) {
  return AEONIAN_VERSION;
}

}  // extern "C"
