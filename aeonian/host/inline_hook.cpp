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
#include <string_view>
#include <vector>

#include "aeonian/api.h"
#include "core/patch_registry.h"
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

// Faixas alteradas pelo motor. Escritas (hook/patch) em modo exclusivo,
// leituras (ReadOriginal, de qualquer thread) em modo compartilhado. Nunca
// destruido: pode ser consultado ate o fim do processo.
SRWLOCK g_registry_lock = SRWLOCK_INIT;

PatchRegistry& Registry() {
  static PatchRegistry* registry = new PatchRegistry();
  return *registry;
}

class RegistryWriter {
 public:
  RegistryWriter() { AcquireSRWLockExclusive(&g_registry_lock); }
  ~RegistryWriter() { ReleaseSRWLockExclusive(&g_registry_lock); }
  RegistryWriter(const RegistryWriter&) = delete;
  RegistryWriter& operator=(const RegistryWriter&) = delete;
};

std::string_view OwnerOf(const char* owner) {
  return owner != nullptr ? owner : "?";
}

void LogConflict(const char* what, uintptr_t target, const PatchRange& other) {
  log::Writef(log::Level::kWarn,
              "%s em 0x%08lx recusado: cruza 0x%08lx..0x%08lx, ja alterado "
              "por %s (%s)",
              what, static_cast<unsigned long>(target),
              static_cast<unsigned long>(other.address),
              static_cast<unsigned long>(other.end()), other.owners.c_str(),
              other.kind == PatchKind::kHook ? "hook" : "patch");
}

// Segundo hook no mesmo alvo. O hook anterior ja leva `target` ao detour
// dele, que tem a convencao da funcao: esse detour vira o "original" do novo
// hook e so o rel32 do jmp muda. Os trampolins anteriores seguem validos.
int ChainHook(uintptr_t target, const PatchRange& hooked, void* detour,
              void** trampoline, std::string_view owner) {
  void* previous = reinterpret_cast<void*>(hooked.detour);
  uint8_t jump[5];
  jump[0] = kOpJmpRel32;
  WriteRel32(jump + 1, static_cast<uint32_t>(target) + 5,
             static_cast<uint32_t>(reinterpret_cast<uintptr_t>(detour)));
  if (!WriteCode(target, jump, sizeof(jump))) return AEONIAN_ERR_MEMORY;
  Registry().Rechain(target, jump, sizeof(jump),
                     reinterpret_cast<uintptr_t>(detour), owner);
  *trampoline = previous;
  log::Writef(log::Level::kInfo,
              "hook inline encadeado: alvo 0x%08lx -> detour %p, que chama o "
              "anterior %p (%s)",
              static_cast<unsigned long>(target), detour, previous,
              hooked.owners.c_str());
  return AEONIAN_OK;
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
               const uint8_t* replacement, uint32_t length, const char* owner) {
  if (target == 0 || replacement == nullptr || length == 0 ||
      length > kMaxPatchLength) {
    return AEONIAN_ERR_ARGUMENT;
  }
  if (!IsReadable(target, length)) return AEONIAN_ERR_MEMORY;
  const std::string_view who = OwnerOf(owner);
  RegistryWriter guard;
  const PatchRange* other = nullptr;
  if (Registry().Check(target, length, PatchKind::kBytes, who, &other) ==
      WriteVerdict::kConflict) {
    LogConflict("patch", target, *other);
    return AEONIAN_ERR_CONFLICT;
  }
  // Sobre patches do proprio mod a conferencia e com a memoria atual: ele
  // sabe o que escreveu (ex.: desfazer o proprio patch).
  if (expected != nullptr &&
      std::memcmp(reinterpret_cast<const void*>(target), expected, length) !=
          0) {
    log::Writef(log::Level::kWarn,
                "patch em 0x%08lx recusado: bytes esperados nao conferem",
                static_cast<unsigned long>(target));
    return AEONIAN_ERR_MISMATCH;
  }
  std::vector<uint8_t> before(length);
  std::memcpy(before.data(), reinterpret_cast<const void*>(target), length);
  if (!WriteCode(target, replacement, length)) return AEONIAN_ERR_MEMORY;
  Registry().Record(target, before.data(), replacement, length,
                    PatchKind::kBytes, who);
  return AEONIAN_OK;
}

int InstallInlineHook(uintptr_t target, const uint8_t* expected,
                      uint32_t length, void* detour, void** trampoline,
                      const char* owner) {
  if (target == 0 || expected == nullptr || detour == nullptr ||
      trampoline == nullptr || length < kMinHookLength ||
      length > kMaxHookLength) {
    return AEONIAN_ERR_ARGUMENT;
  }
  if (!IsReadable(target, length)) return AEONIAN_ERR_MEMORY;
  const std::string_view who = OwnerOf(owner);
  RegistryWriter guard;
  const PatchRange* hooked = nullptr;
  const WriteVerdict verdict =
      Registry().Check(target, length, PatchKind::kHook, who, &hooked);
  if (verdict == WriteVerdict::kConflict) {
    LogConflict("hook", target, *hooked);
    return AEONIAN_ERR_CONFLICT;
  }
  // Verificacao ANTES de qualquer escrita, contra os bytes ORIGINAIS: numa
  // build diferente a funcao tem outros bytes e o hook e recusado em vez de
  // corromper o jogo; num alvo ja desviado a memoria tem o jmp anterior.
  uint8_t current[kMaxHookLength];
  std::memcpy(current, reinterpret_cast<const void*>(target), length);
  Registry().RestoreOriginal(target, current, length);
  if (std::memcmp(current, expected, length) != 0) {
    log::Writef(log::Level::kWarn,
                "hook em 0x%08lx recusado: bytes esperados nao conferem%s",
                static_cast<unsigned long>(target),
                verdict == WriteVerdict::kChain
                    ? " com os originais (alvo ja desviado: passe os bytes "
                      "da spec ou de read_original)"
                    : "");
    return AEONIAN_ERR_MISMATCH;
  }
  if (verdict == WriteVerdict::kChain) {
    return ChainHook(target, *hooked, detour, trampoline, who);
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
  Registry().Record(target, expected, patch, length, PatchKind::kHook, who,
                    reinterpret_cast<uintptr_t>(detour));
  *trampoline = tramp;
  log::Writef(log::Level::kInfo,
              "hook inline instalado: alvo 0x%08lx (%u bytes) -> detour %p",
              static_cast<unsigned long>(target), length, detour);
  return AEONIAN_OK;
}

int ReadOriginal(uintptr_t address, uint8_t* out, uint32_t length,
                 std::string* owners) {
  if (address == 0 || out == nullptr || length == 0 ||
      length > kMaxPatchLength) {
    return AEONIAN_ERR_ARGUMENT;
  }
  if (!IsReadable(address, length)) return AEONIAN_ERR_MEMORY;
  AcquireSRWLockShared(&g_registry_lock);
  std::memcpy(out, reinterpret_cast<const void*>(address), length);
  Registry().RestoreOriginal(address, out, length, owners);
  ReleaseSRWLockShared(&g_registry_lock);
  return AEONIAN_OK;
}

}  // namespace aeonian
