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

// aeonian/core/pe_image.h
//
// Leitura minima de uma imagem PE32 ja mapeada em memoria (layout de RVAs):
// identificacao da build e localizacao de entradas da IAT. Sem Win32.

#ifndef AEONIAN_CORE_PE_IMAGE_H_
#define AEONIAN_CORE_PE_IMAGE_H_

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace aeonian {

struct PeInfo {
  uint16_t machine = 0;
  uint32_t timestamp = 0;
  uint32_t size_of_image = 0;
  uint32_t entry_rva = 0;
  uint32_t image_base = 0;
  uint32_t checksum = 0;
};

// Valida MZ/PE32 i386 e preenche `out`. `size` e o tamanho legivel da
// imagem (SizeOfImage no uso real).
bool ParsePeHeaders(const uint8_t* base, size_t size, PeInfo* out);

// Endereco da entrada da IAT (32 bits) de `func` importada de `dll`
// (`dll` sem diferenciar maiusculas; `func` pelo nome exato). nullptr se nao
// importada ou se a imagem estiver mal formada. `base` aponta o inicio da
// imagem mapeada.
uint32_t* FindImportSlot(uint8_t* base, size_t size, std::string_view dll,
                         std::string_view func);

}  // namespace aeonian

#endif  // AEONIAN_CORE_PE_IMAGE_H_
