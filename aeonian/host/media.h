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

// aeonian/host/media.h
//
// Videos do jogo (Bink e Smacker). O Game.exe os abre por `BinkOpen` e
// `SmackOpen`, importados de binkw32.dll/smackw32.dll. Interceptando essas
// duas chamadas o Aeonian consegue (a) registrar quais videos o jogo pede e
// (b) pular os de abertura, independentemente do renderizador (GDI,
// DirectDraw ou D2GL); o jogo trata "nao consegui abrir" como video ausente.

#ifndef AEONIAN_HOST_MEDIA_H_
#define AEONIAN_HOST_MEDIA_H_

#include <windows.h>

#include <cstdint>

namespace aeonian {

using BinkOpenFn = void*(WINAPI*)(const char* name, uint32_t flags);
using SmackOpenFn = void*(WINAPI*)(const char* name, uint32_t size_flags, uint32_t extra);

void* WINAPI HookBinkOpen(const char* name, uint32_t flags);
void* WINAPI HookSmackOpen(const char* name, uint32_t size_flags, uint32_t extra);
extern BinkOpenFn g_real_bink_open;
extern SmackOpenFn g_real_smack_open;

}  // namespace aeonian

#endif  // AEONIAN_HOST_MEDIA_H_
