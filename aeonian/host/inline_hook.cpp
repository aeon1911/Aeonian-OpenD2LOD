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

#include "host/inline_hook.h"

#include <windows.h>

#include <cstring>

#include "aeonian/api.h"
#include "host/log.h"

static_assert(sizeof(void*) == 4, "o Aeonian mira o Game.exe de 32 bits");

namespace aeonian {

namespace {

constexpr uint8_t kOpJmpRel32 = 0xE9;
constexpr uint8_t kOpCallRel32 = 0xE8;
constexpr uint8_t kOpNop = 0x90;

void WriteRel32(uint8_t* at, uint32_t from_next_ip, uint32_t to) {
  uint32_t rel = to - from_next_ip;
  std::memcpy(at, &rel, 4);
}

// Protege/escreve/restaura. `bytes` pode ser nulo para so trocar a protecao.
bool WriteCode(uintptr_t target, const uint8_t* bytes, uint32_t length) {
  DWORD old = 0;
  if (!VirtualProtect(reinterpret_cast<void*>(target), length,
                      PAGE_EXECUTE_READWRITE, &old)) {
    return false;
  }
  std::memcpy(reinterpret_cast<void*>(target), bytes, length);
  DWORD ignored = 0;
  VirtualProtect(reinterpret_cast<void*>(target), length, old, &ignored);
  FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target),
                        length);
  return true;
}

}  // namespace

bool IsReadable(uintptr_t address, uint32_t length) {
  uintptr_t cursor = address;
  uintptr_t end = address + length;
  if (end < address) return false;
  while (cursor < end) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(cursor), &mbi, sizeof(mbi)) == 0) {
      return false;
    }
    if (mbi.State != MEM_COMMIT) return false;
    if ((mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
    uintptr_t region_end =
        reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
    if (region_end <= cursor) return false;
    cursor = region_end;
  }
  return true;
}

int PatchBytes(uintptr_t target, const uint8_t* expected,
               const uint8_t* replacement, uint32_t length) {
  if (target == 0 || replacement == nullptr || length == 0 ||
      length > 4096) {
    return AEONIAN_ERR_ARGUMENT;
  }
  if (!IsReadable(target, length)) return AEONIAN_ERR_MEMORY;
  if (expected != nullptr &&
      std::memcmp(reinterpret_cast<const void*>(target), expected, length) !=
          0) {
    log::Writef(log::Level::kWarn,
                "patch em 0x%08lx recusado: bytes esperados nao conferem",
                static_cast<unsigned long>(target));
    return AEONIAN_ERR_MISMATCH;
  }
  return WriteCode(target, replacement, length) ? AEONIAN_OK
                                                : AEONIAN_ERR_MEMORY;
}

int InstallInlineHook(uintptr_t target, const uint8_t* expected,
                      uint32_t length, void* detour, void** trampoline) {
  if (target == 0 || expected == nullptr || detour == nullptr ||
      trampoline == nullptr || length < kMinHookLength ||
      length > kMaxHookLength) {
    return AEONIAN_ERR_ARGUMENT;
  }
  if (!IsReadable(target, length)) return AEONIAN_ERR_MEMORY;
  // Verificacao ANTES de qualquer escrita: numa build diferente a funcao tem
  // outros bytes e o hook e recusado em vez de corromper o jogo.
  if (std::memcmp(reinterpret_cast<const void*>(target), expected, length) !=
      0) {
    log::Writef(log::Level::kWarn,
                "hook em 0x%08lx recusado: bytes esperados nao conferem",
                static_cast<unsigned long>(target));
    return AEONIAN_ERR_MISMATCH;
  }

  // Trampolim = copia dos bytes originais + `jmp target+length`. Escrito
  // como RW e so entao promovido a RX (nao exige paginas RWX).
  const uint32_t tramp_size = length + 5;
  uint8_t* tramp = static_cast<uint8_t*>(
      VirtualAlloc(nullptr, tramp_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  if (tramp == nullptr) return AEONIAN_ERR_MEMORY;
  std::memcpy(tramp, expected, length);
  if (expected[0] == kOpCallRel32 || expected[0] == kOpJmpRel32) {
    // O primeiro byte e um desvio relativo: reaponta para o mesmo destino.
    int32_t old_rel;
    std::memcpy(&old_rel, expected + 1, 4);
    uint32_t destination = static_cast<uint32_t>(target) + 5 +
                           static_cast<uint32_t>(old_rel);
    WriteRel32(tramp + 1, static_cast<uint32_t>(
                              reinterpret_cast<uintptr_t>(tramp)) + 5,
               destination);
  }
  tramp[length] = kOpJmpRel32;
  WriteRel32(tramp + length + 1,
             static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tramp)) +
                 tramp_size,
             static_cast<uint32_t>(target) + length);
  DWORD old_protect = 0;
  if (!VirtualProtect(tramp, tramp_size, PAGE_EXECUTE_READ, &old_protect)) {
    VirtualFree(tramp, 0, MEM_RELEASE);
    return AEONIAN_ERR_MEMORY;
  }
  FlushInstructionCache(GetCurrentProcess(), tramp, tramp_size);

  // Desvio no alvo: jmp rel32 + NOPs ate `length`.
  uint8_t patch[kMaxHookLength];
  std::memset(patch, kOpNop, sizeof(patch));
  patch[0] = kOpJmpRel32;
  WriteRel32(patch + 1, static_cast<uint32_t>(target) + 5,
             static_cast<uint32_t>(reinterpret_cast<uintptr_t>(detour)));
  if (!WriteCode(target, patch, length)) {
    VirtualFree(tramp, 0, MEM_RELEASE);
    return AEONIAN_ERR_MEMORY;
  }
  *trampoline = tramp;
  log::Writef(log::Level::kInfo,
              "hook inline instalado: alvo 0x%08lx (%u bytes) -> detour %p",
              static_cast<unsigned long>(target), length, detour);
  return AEONIAN_OK;
}

}  // namespace aeonian
