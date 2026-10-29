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

// aeonian/core/mod_manifest.h
//
// Manifesto `mod.ini` de um mod:
//
//   [mod]
//   id       = meu-hud            ; [a-z0-9._-]; padrao: nome da pasta
//   name     = Meu HUD
//   version  = 1.0.0
//   author   = Fulano
//   priority = 100                ; maior vence conflitos de arquivo
//   plugin   = hud.dll            ; opcional, lista separada por virgula
//   args     = -txt               ; opcional, flags extras de linha de comando
//   builds   = 1.14d              ; opcional, builds suportadas
//
// Sem Win32; compila no host para os testes.

#ifndef AEONIAN_CORE_MOD_MANIFEST_H_
#define AEONIAN_CORE_MOD_MANIFEST_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aeonian {

struct ModManifest {
  std::string id;
  std::string name;
  std::string version;
  std::string author;
  int priority = 0;
  std::vector<std::string> plugins;  // relativos a `dir`
  std::vector<std::string> args;
  std::vector<std::string> builds;  // vazio = qualquer build
  std::string dir;                  // pasta do mod (preenchido por quem carrega)
};

bool IsValidModId(std::string_view id);

// `dir_name` (nome da pasta) vira o id quando o manifesto nao o define.
// Em caso de erro devolve nullopt e descreve o motivo em `error`.
std::optional<ModManifest> ParseModManifest(std::string_view text,
                                            std::string_view dir_name,
                                            std::string* error);

// Ordem de carga: maior prioridade primeiro; empate pelo id (determinista).
void SortForLoad(std::vector<ModManifest>* mods);

// O mod declara suporte a `build_name` (ou nao restringe)?
bool SupportsBuild(const ModManifest& mod, std::string_view build_name);

}  // namespace aeonian

#endif  // AEONIAN_CORE_MOD_MANIFEST_H_
