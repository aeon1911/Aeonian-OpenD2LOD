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

// aeonian/core/ini.h
//
// Leitor de INI minimo usado por aeonian.ini, mod.ini e pelos bancos de
// enderecos. Sem Win32; compila no host para os testes.
//
// Regras: `[secao]`, `chave = valor`; comentarios de linha com `;` ou `#`;
// comentario no fim da linha so quando precedido de espaco (` ;`); valor
// entre aspas duplas e mantido literal. Secao e chave nao diferenciam
// maiusculas de minusculas.

#ifndef AEONIAN_CORE_INI_H_
#define AEONIAN_CORE_INI_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace aeonian {

std::string ToLowerAscii(std::string_view text);
std::string_view Trim(std::string_view text);

// Decimal (com sinal) ou hexadecimal `0x...`. nullopt se invalido/vazio.
std::optional<int64_t> ParseInt(std::string_view text);

// Divide por virgula e/ou espaco, descartando itens vazios.
std::vector<std::string> SplitList(std::string_view text);

class Ini {
 public:
  using Entries = std::vector<std::pair<std::string, std::string>>;

  static Ini Parse(std::string_view text);

  bool HasSection(std::string_view section) const;
  std::optional<std::string> Get(std::string_view section,
                                 std::string_view key) const;
  std::string GetOr(std::string_view section, std::string_view key,
                    std::string_view fallback) const;
  bool GetBool(std::string_view section, std::string_view key,
               bool fallback) const;
  int64_t GetInt(std::string_view section, std::string_view key,
                 int64_t fallback) const;

  // Nomes das secoes na ordem do arquivo, com a caixa original.
  const std::vector<std::string>& SectionNames() const { return names_; }
  // Todas as entradas de uma secao (chaves em minusculas), na ordem.
  Entries Section(std::string_view section) const;

 private:
  struct SectionData {
    std::string key;  // minusculo
    Entries entries;
  };
  SectionData* FindOrAdd(std::string_view name);
  const SectionData* Find(std::string_view name) const;

  std::vector<SectionData> sections_;
  std::vector<std::string> names_;
};

}  // namespace aeonian

#endif  // AEONIAN_CORE_INI_H_
