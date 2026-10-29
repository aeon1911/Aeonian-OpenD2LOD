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

// aeonian/core/vfs.h
//
// Sistema de arquivos virtual dos mods. O jogo (em modo `-direct`) pede
// arquivos soltos em `<jogo>\data\...` antes de olhar os MPQs. O Vfs decide,
// para cada pedido, se algum mod fornece aquele arquivo: o mod de maior
// prioridade vence. Nada e copiado nem alterado no disco do usuario.
//
// Logica pura (sem Win32): a existencia dos arquivos entra por callback, o
// que permite testar no host.

#ifndef AEONIAN_CORE_VFS_H_
#define AEONIAN_CORE_VFS_H_

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace aeonian {

struct ModRoot {
  std::string id;
  std::string dir;  // pasta do mod, sem barra final
};

class Vfs {
 public:
  using ExistsFn = std::function<bool(const std::string& path)>;

  // `roots` em ordem de prioridade: o primeiro elemento vence os demais.
  Vfs(std::string game_dir, std::vector<ModRoot> roots, ExistsFn exists);

  // Se `requested` aponta para um arquivo sob `<jogo>\data\` que algum mod
  // fornece, devolve o caminho dentro do mod (separadores `\`). Caso
  // contrario devolve nullopt e o pedido segue intacto. Preenche `owner` com
  // o id do mod vencedor quando nao for nulo.
  std::optional<std::string> Resolve(std::string_view requested,
                                     std::string* owner = nullptr) const;

  // Parte do caminho depois de `data\`, normalizada (separadores `\`, sem
  // `.`/`..`), ou nullopt se `requested` nao estiver sob `<jogo>\data\`.
  std::optional<std::string> RelativeDataPath(
      std::string_view requested) const;

  const std::string& game_dir() const { return game_dir_; }
  const std::vector<ModRoot>& roots() const { return roots_; }

 private:
  std::string game_dir_;
  std::vector<ModRoot> roots_;
  ExistsFn exists_;
};

// Normaliza separadores para `\` e remove barras finais.
std::string NormalizeDir(std::string_view dir);

}  // namespace aeonian

#endif  // AEONIAN_CORE_VFS_H_
