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

// aeonian/host/hooks.h
//
// Hooks na tabela de importacao (IAT) do Game.exe. Interceptam so as chamadas
// que o PROPRIO executavel faz, sem alterar codigo nem as DLLs do sistema, e
// nao dependem de enderecos internos da build.

#ifndef AEONIAN_HOST_HOOKS_H_
#define AEONIAN_HOST_HOOKS_H_

#include <cstddef>
#include <cstdint>

namespace aeonian {

// Instala os hooks na imagem mapeada do Game.exe. Registra no log cada
// importacao interceptada (ou ausente).
void InstallHooks(uint8_t* image_base, size_t image_size);

}  // namespace aeonian

#endif  // AEONIAN_HOST_HOOKS_H_
