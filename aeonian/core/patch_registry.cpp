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

#include "core/patch_registry.h"

#include <algorithm>
#include <cstring>

namespace aeonian {

namespace {

bool Overlaps(const PatchRange& r, uintptr_t address, uintptr_t end) {
  return r.address < end && address < r.end();
}

}  // namespace

WriteVerdict PatchRegistry::Check(uintptr_t address, size_t length,
                                  PatchKind kind, std::string_view owner,
                                  const PatchRange** match) const {
  if (match != nullptr) *match = nullptr;
  const uintptr_t end = address + length;
  const PatchRange* first = nullptr;
  const PatchRange* foreign = nullptr;  // 1a faixa que nao e patch deste mod
  size_t overlaps = 0;
  for (const PatchRange& r : ranges_) {
    if (!Overlaps(r, address, end)) continue;
    ++overlaps;
    if (first == nullptr) first = &r;
    if (foreign == nullptr &&
        (r.kind != PatchKind::kBytes || r.owners != owner)) {
      foreign = &r;
    }
  }
  if (overlaps == 0) return WriteVerdict::kFree;
  if (kind == PatchKind::kHook && overlaps == 1 &&
      first->kind == PatchKind::kHook && first->address == address) {
    if (match != nullptr) *match = first;
    return WriteVerdict::kChain;
  }
  if (kind == PatchKind::kBytes && foreign == nullptr) {
    return WriteVerdict::kOwnBytes;
  }
  if (match != nullptr) *match = foreign != nullptr ? foreign : first;
  return WriteVerdict::kConflict;
}

size_t PatchRegistry::RestoreOriginal(uintptr_t address, uint8_t* bytes,
                                      size_t length,
                                      std::string* owners) const {
  const uintptr_t end = address + length;
  size_t restored = 0;
  for (const PatchRange& r : ranges_) {
    if (!Overlaps(r, address, end)) continue;
    const uintptr_t lo = std::max(r.address, address);
    const uintptr_t hi = std::min(r.end(), end);
    uint8_t* window = bytes + (lo - address);
    const size_t offset = lo - r.address;
    const size_t count = hi - lo;
    if (std::memcmp(window, r.written.data() + offset, count) != 0) continue;
    std::memcpy(window, r.original.data() + offset, count);
    restored += count;
    if (owners != nullptr) {
      if (!owners->empty()) *owners += ", ";
      *owners += r.owners;
    }
  }
  return restored;
}

void PatchRegistry::Record(uintptr_t address, const uint8_t* before,
                           const uint8_t* after, size_t length, PatchKind kind,
                           std::string_view owner, uintptr_t detour) {
  if (length == 0) return;
  const uintptr_t end = address + length;
  // As faixas sao disjuntas: a uniao da escrita nova com as que ela cruza e
  // um intervalo so.
  PatchRange merged;
  merged.address = address;
  merged.kind = kind;
  merged.owners = std::string(owner);
  merged.detour = detour;
  uintptr_t merged_end = end;
  for (const PatchRange& r : ranges_) {
    if (!Overlaps(r, address, end)) continue;
    merged.address = std::min(merged.address, r.address);
    merged_end = std::max(merged_end, r.end());
  }
  const size_t size = merged_end - merged.address;
  merged.original.resize(size);
  merged.written.resize(size);
  const size_t at = address - merged.address;
  std::memcpy(merged.original.data() + at, before, length);
  std::memcpy(merged.written.data() + at, after, length);
  // O original de um byte ja alterado e o da faixa antiga, nunca o `before`
  // (que e a escrita anterior do proprio motor).
  for (const PatchRange& r : ranges_) {
    if (!Overlaps(r, address, end)) continue;
    for (size_t i = 0; i < r.original.size(); ++i) {
      const uintptr_t byte = r.address + i;
      const size_t j = byte - merged.address;
      merged.original[j] = r.original[i];
      if (byte < address || byte >= end) merged.written[j] = r.written[i];
    }
  }
  ranges_.erase(std::remove_if(ranges_.begin(), ranges_.end(),
                               [&](const PatchRange& r) {
                                 return Overlaps(r, address, end);
                               }),
                ranges_.end());
  // Um patch desfeito pelo proprio mod deixa de ser uma alteracao.
  if (kind == PatchKind::kBytes && merged.original == merged.written) return;
  auto pos = std::lower_bound(
      ranges_.begin(), ranges_.end(), merged.address,
      [](const PatchRange& r, uintptr_t a) { return r.address < a; });
  ranges_.insert(pos, std::move(merged));
}

void PatchRegistry::Rechain(uintptr_t address, const uint8_t* jump,
                            size_t jump_length, uintptr_t detour,
                            std::string_view owner) {
  for (PatchRange& r : ranges_) {
    if (r.kind != PatchKind::kHook || r.address != address) continue;
    std::memcpy(r.written.data(), jump, std::min(jump_length, r.written.size()));
    r.detour = detour;
    r.owners += " > ";
    r.owners += owner;
    return;
  }
}

}  // namespace aeonian
