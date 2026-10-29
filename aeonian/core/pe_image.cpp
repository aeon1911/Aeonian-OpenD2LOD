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

#include "core/pe_image.h"

#include <cstring>

#include "core/ini.h"

namespace aeonian {

namespace {

// Offsets do formato PE32 (documentados publicamente pela Microsoft).
constexpr size_t kPeSignatureOffsetField = 0x3C;
constexpr uint32_t kPeSignature = 0x00004550;  // "PE\0\0"
constexpr uint16_t kMachineI386 = 0x014C;
constexpr uint16_t kOptionalMagicPe32 = 0x010B;
constexpr size_t kDataDirectoryImport = 1;

template <typename T>
bool ReadAt(const uint8_t* base, size_t size, size_t offset, T* out) {
  if (offset > size || sizeof(T) > size - offset) return false;
  std::memcpy(out, base + offset, sizeof(T));
  return true;
}

bool ReadCString(const uint8_t* base, size_t size, size_t offset,
                 std::string_view* out) {
  if (offset >= size) return false;
  size_t end = offset;
  while (end < size && base[end] != 0) ++end;
  if (end == size) return false;  // sem terminador dentro da imagem
  *out = std::string_view(reinterpret_cast<const char*>(base + offset),
                          end - offset);
  return true;
}

}  // namespace

bool ParsePeHeaders(const uint8_t* base, size_t size, PeInfo* out) {
  uint16_t mz = 0;
  if (!ReadAt(base, size, 0, &mz) || mz != 0x5A4D) return false;
  uint32_t lfanew = 0;
  if (!ReadAt(base, size, kPeSignatureOffsetField, &lfanew)) return false;
  uint32_t sig = 0;
  if (!ReadAt(base, size, lfanew, &sig) || sig != kPeSignature) return false;

  PeInfo info;
  size_t file_header = lfanew + 4;
  size_t optional = file_header + 20;
  uint16_t magic = 0;
  if (!ReadAt(base, size, file_header + 0, &info.machine) ||
      !ReadAt(base, size, file_header + 4, &info.timestamp) ||
      !ReadAt(base, size, optional, &magic) ||
      !ReadAt(base, size, optional + 16, &info.entry_rva) ||
      !ReadAt(base, size, optional + 28, &info.image_base) ||
      !ReadAt(base, size, optional + 56, &info.size_of_image) ||
      !ReadAt(base, size, optional + 64, &info.checksum)) {
    return false;
  }
  if (info.machine != kMachineI386 || magic != kOptionalMagicPe32) return false;
  *out = info;
  return true;
}

uint32_t* FindImportSlot(uint8_t* base, size_t size, std::string_view dll,
                         std::string_view func) {
  uint32_t lfanew = 0;
  if (!ReadAt(base, size, kPeSignatureOffsetField, &lfanew)) return nullptr;
  size_t optional = static_cast<size_t>(lfanew) + 4 + 20;
  uint32_t import_rva = 0;
  uint32_t import_size = 0;
  size_t dir = optional + 96 + 8 * kDataDirectoryImport;
  if (!ReadAt(base, size, dir, &import_rva) ||
      !ReadAt(base, size, dir + 4, &import_size) || import_rva == 0) {
    return nullptr;
  }

  const std::string wanted_dll = ToLowerAscii(dll);
  // IMAGE_IMPORT_DESCRIPTOR: OriginalFirstThunk, TimeDateStamp,
  // ForwarderChain, Name, FirstThunk (5 x uint32).
  for (size_t desc = import_rva;; desc += 20) {
    uint32_t original_first = 0, name_rva = 0, first_thunk = 0;
    if (!ReadAt(base, size, desc, &original_first) ||
        !ReadAt(base, size, desc + 12, &name_rva) ||
        !ReadAt(base, size, desc + 16, &first_thunk)) {
      return nullptr;
    }
    if (name_rva == 0 && first_thunk == 0) return nullptr;  // fim da tabela

    std::string_view name;
    if (!ReadCString(base, size, name_rva, &name)) continue;
    if (ToLowerAscii(name) != wanted_dll) continue;

    size_t lookup = original_first != 0 ? original_first : first_thunk;
    for (size_t index = 0;; ++index) {
      uint32_t entry = 0;
      if (!ReadAt(base, size, lookup + 4 * index, &entry) || entry == 0) break;
      if ((entry & 0x80000000u) != 0) continue;  // importada por ordinal
      std::string_view fname;
      if (!ReadCString(base, size, static_cast<size_t>(entry) + 2, &fname)) {
        continue;  // +2: pula o campo Hint
      }
      if (fname == func) {
        size_t slot = static_cast<size_t>(first_thunk) + 4 * index;
        if (slot + 4 > size) return nullptr;
        return reinterpret_cast<uint32_t*>(base + slot);
      }
    }
  }
}

}  // namespace aeonian
