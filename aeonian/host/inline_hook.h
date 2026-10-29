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

#ifndef AEONIAN_HOST_INLINE_HOOK_H_
#define AEONIAN_HOST_INLINE_HOOK_H_

#include <cstdint>

namespace aeonian {

constexpr uint32_t kMinHookLength = 5;
constexpr uint32_t kMaxHookLength = 32;

// Retornam AEONIAN_OK ou um AEONIAN_ERR_* (aeonian/api.h).
int InstallInlineHook(uintptr_t target, const uint8_t* expected,
                      uint32_t length, void* detour, void** trampoline);
int PatchBytes(uintptr_t target, const uint8_t* expected,
               const uint8_t* replacement, uint32_t length);

// A regiao [address, address+length) esta mapeada e legivel?
bool IsReadable(uintptr_t address, uint32_t length);

}  // namespace aeonian

#endif  // AEONIAN_HOST_INLINE_HOOK_H_
