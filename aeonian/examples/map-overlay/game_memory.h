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

// Leitura protegida da memoria do proprio processo (Win32, i686), para a
// logica de map_logic.h. So copia de paginas comprometidas e legiveis; um
// ponteiro ruim vira "false", nunca uma excecao dentro do jogo. As regioes
// consultadas ficam num cache curto: chame Reset() a cada passagem (o mapa de
// memoria muda entre elas). OriginalMemory le os bytes do jogo como estavam
// antes de qualquer hook, para as ancoras do banco de enderecos.

#ifndef AEONIAN_EXAMPLES_MAP_OVERLAY_GAME_MEMORY_H_
#define AEONIAN_EXAMPLES_MAP_OVERLAY_GAME_MEMORY_H_

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "aeonian/api.h"
#include "map_logic.h"

namespace map_overlay {

class GameMemory final : public MemoryView {
 public:
  bool Read(uint32_t address, void* out, uint32_t size) const override {
    if (address < 0x10000 || size == 0 || address + size < address) return false;
    if (!Readable(address, address + size)) return false;
    std::memcpy(out, reinterpret_cast<const void*>(static_cast<uintptr_t>(address)), size);
    return true;
  }
  void Reset() { count_ = 0; }

 private:
  static constexpr uint32_t kSlots = 32;
  bool Readable(uint32_t begin, uint32_t end) const {
    uint32_t cursor = begin;
    while (cursor < end) {
      bool hit = false;
      for (uint32_t i = 0; i < count_; ++i) {
        if (cursor >= begin_[i] && cursor < end_[i]) {
          cursor = end_[i];
          hit = true;
          break;
        }
      }
      if (hit) continue;
      MEMORY_BASIC_INFORMATION mbi;
      if (VirtualQuery(reinterpret_cast<const void*>(static_cast<uintptr_t>(cursor)), &mbi,
                       sizeof(mbi)) == 0) {
        return false;
      }
      const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                             PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                             PAGE_EXECUTE_WRITECOPY;
      if (mbi.State != MEM_COMMIT || (mbi.Protect & readable) == 0 ||
          (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
      }
      const uint32_t region_begin =
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mbi.BaseAddress));
      const uint32_t region_end = region_begin + static_cast<uint32_t>(mbi.RegionSize);
      if (region_end <= cursor) return false;  // inclui o fim do espaco de 32 bits
      const uint32_t slot = count_ < kSlots ? count_++ : (next_++ % kSlots);
      begin_[slot] = region_begin;
      end_[slot] = region_end;
      cursor = region_end;
    }
    return true;
  }
  mutable uint32_t begin_[kSlots] = {};
  mutable uint32_t end_[kSlots] = {};
  mutable uint32_t count_ = 0;
  mutable uint32_t next_ = 0;
};

// Ancoras e `expected` de hooks: numa funcao que outro mod desviou, a memoria
// tem o jmp dele por cima dos bytes verificados. read_original os devolve como
// eram. Num host sem esse campo nao ha registro de desvios e resolve_symbol
// conferiu a memoria atual, entao ela serve (`fallback`).
class OriginalMemory final : public MemoryView {
 public:
  OriginalMemory(const AeonianHost* host, const MemoryView& fallback)
      : host_(host), fallback_(fallback) {}
  bool Read(uint32_t address, void* out, uint32_t size) const override {
    if (host_->struct_size <
        offsetof(AeonianHost, read_original) + sizeof(host_->read_original)) {
      return fallback_.Read(address, out, size);
    }
    return host_->read_original(host_, address, out, size) == AEONIAN_OK;
  }

 private:
  const AeonianHost* host_;
  const MemoryView& fallback_;
};

}  // namespace map_overlay

#endif  // AEONIAN_EXAMPLES_MAP_OVERLAY_GAME_MEMORY_H_
