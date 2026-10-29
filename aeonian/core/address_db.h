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

// aeonian/core/address_db.h
//
// Banco de enderecos de uma build do jogo, em formato de dados
// (aeonian/addresses/<build>.ini). Cada simbolo traz o RVA e, opcionalmente,
// os bytes que devem estar la; o tempo de execucao so entrega o endereco se
// os bytes conferirem. Assim um hook jamais e aplicado numa build errada.
//
//   [build]
//   name          = 1.14d
//   id            = 1                ; AEONIAN_BUILD_*
//   timestamp     = 0x574ddfbc       ; PE TimeDateStamp
//   size_of_image = 0x5ba000
//   entry_rva     = 0x282985
//   sha256        = ...              ; informativo (usado pelo instalador)
//
//   [symbol.Game.EntryPoint]
//   rva         = 0x282985
//   bytes       = e8 1b 9f 00 00 e9 16 fe ff ff
//   description = ...

#ifndef AEONIAN_CORE_ADDRESS_DB_H_
#define AEONIAN_CORE_ADDRESS_DB_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/pe_image.h"

namespace aeonian {

struct Symbol {
  std::string name;  // caixa original
  uint32_t rva = 0;
  std::vector<uint8_t> expect;  // vazio = sem verificacao
  std::string description;
};

class AddressDb {
 public:
  static std::optional<AddressDb> Parse(std::string_view text,
                                        std::string* error);

  const std::string& build_name() const { return build_name_; }
  uint32_t build_id() const { return build_id_; }
  const std::string& sha256() const { return sha256_; }

  // A imagem em memoria e a build descrita por este banco?
  bool Matches(const PeInfo& pe) const;

  // Busca sem diferenciar maiusculas; nullptr se nao existe.
  const Symbol* Find(std::string_view name) const;
  const std::vector<Symbol>& symbols() const { return symbols_; }

 private:
  std::string build_name_;
  uint32_t build_id_ = 0;
  std::string sha256_;
  uint32_t timestamp_ = 0;
  uint32_t size_of_image_ = 0;
  uint32_t entry_rva_ = 0;
  std::vector<Symbol> symbols_;
};

// "e8 1b 9f" -> {0xE8, 0x1B, 0x9F}. nullopt se invalido.
std::optional<std::vector<uint8_t>> ParseHexBytes(std::string_view text);

}  // namespace aeonian

#endif  // AEONIAN_CORE_ADDRESS_DB_H_
