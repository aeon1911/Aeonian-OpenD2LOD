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

// aeonian/proxy/ijl11_proxy.cpp
//
// Ponto de entrada nao destrutivo. O Game.exe 1.14d importa `ijl11.dll` (a
// biblioteca JPEG da Intel, usada nos screenshots). O instalador renomeia a
// DLL original para `ijl11_real.dll` e coloca esta no lugar: ela encaminha as
// seis exportacoes (ver ijl11.def) para a original e, ao ser carregada, sobe
// o Aeonian. O Game.exe NAO e modificado no disco.
//
// Esta DLL nao depende de aeonian.dll: se ele faltar (ou se AEONIAN_DISABLE=1
// estiver no ambiente) o jogo inicia normalmente, como o original.

#include <windows.h>

namespace {

using BootstrapFn = int(__cdecl*)(void);

bool Disabled() {
  char buf[8];
  DWORD n = GetEnvironmentVariableA("AEONIAN_DISABLE", buf, sizeof(buf));
  return n > 0 && n < sizeof(buf) && buf[0] != '0';
}

void StartAeonian(HMODULE self) {
  if (Disabled()) return;
  char path[MAX_PATH * 2];
  DWORD n = GetModuleFileNameA(self, path, sizeof(path));
  if (n == 0 || n >= sizeof(path)) return;
  while (n > 0 && path[n - 1] != '\\' && path[n - 1] != '/') --n;
  static const char kRelative[] = "aeonian\\aeonian.dll";
  if (n + sizeof(kRelative) > sizeof(path)) return;
  for (size_t i = 0; i < sizeof(kRelative); ++i) path[n + i] = kRelative[i];

  HMODULE core =
      LoadLibraryExA(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (core == nullptr) return;
  FARPROC raw = GetProcAddress(core, "AeonianBootstrap");
  if (raw == nullptr) return;
  BootstrapFn bootstrap = nullptr;
  static_assert(sizeof(bootstrap) == sizeof(raw), "ponteiro de funcao");
  __builtin_memcpy(&bootstrap, &raw, sizeof(bootstrap));
  bootstrap();
}

}  // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);
    StartAeonian(instance);
  }
  return TRUE;
}
