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

// aeonian/host/inline_hook.h
//
// Hooks inline (x86 de 32 bits) e escrita de bytes, SEMPRE precedidos da
// verificacao dos bytes esperados. Contrato em aeonian/include/aeonian/api.h.
//
// Toda escrita fica num registro (core/patch_registry.h) com os bytes que
// estavam la antes, de modo que um alvo ja desviado continua conferivel
// contra os bytes originais (ReadOriginal, resolve_symbol).

#ifndef AEONIAN_HOST_INLINE_HOOK_H_
#define AEONIAN_HOST_INLINE_HOOK_H_

#include <cstdint>
#include <string>

namespace aeonian {

constexpr uint32_t kMinHookLength = 5;
constexpr uint32_t kMaxHookLength = 32;
constexpr uint32_t kMaxPatchLength = 4096;

// Retornam AEONIAN_OK ou um AEONIAN_ERR_* (aeonian/api.h). `owner` e o id do
// mod que pediu (para o registro e o log).
//
// Hook no MESMO inicio de um hook ja instalado: encadeia. `expected` e
// conferido contra os bytes originais, o desvio passa a levar ao novo detour
// e `*trampoline` recebe o detour anterior (que acaba chamando o original).
// Qualquer outra sobreposicao com uma faixa ja alterada: AEONIAN_ERR_CONFLICT.
int InstallInlineHook(uintptr_t target, const uint8_t* expected,
                      uint32_t length, void* detour, void** trampoline,
                      const char* owner);
// Patch que cruza um hook ou um patch de outro mod: AEONIAN_ERR_CONFLICT.
// Sobre patches do proprio mod, `expected` confere com a memoria atual.
int PatchBytes(uintptr_t target, const uint8_t* expected,
               const uint8_t* replacement, uint32_t length, const char* owner);

// Copia [address, address+length) como estava antes de qualquer escrita do
// motor. `owners` (opcional) recebe os mods que alteraram a faixa.
int ReadOriginal(uintptr_t address, uint8_t* out, uint32_t length,
                 std::string* owners);

// A regiao [address, address+length) esta mapeada e legivel?
bool IsReadable(uintptr_t address, uint32_t length);

}  // namespace aeonian

#endif  // AEONIAN_HOST_INLINE_HOOK_H_
