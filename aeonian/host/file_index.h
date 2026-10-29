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

// aeonian/host/file_index.h
//
// Indice, montado uma vez na inicializacao, dos arquivos que os mods trazem
// em `<mod>\data\`. Com ele cada abertura de arquivo do jogo custa uma busca
// em memoria em vez de uma chamada ao sistema por mod (o que importa em
// midias lentas, como o cartao SD de um console). Consequencia: arquivos
// NOVOS adicionados a um mod com o jogo aberto so valem apos reiniciar;
// editar o conteudo de um arquivo ja indexado vale na proxima leitura.

#ifndef AEONIAN_HOST_FILE_INDEX_H_
#define AEONIAN_HOST_FILE_INDEX_H_

#include <cstddef>
#include <string>
#include <unordered_set>

namespace aeonian {

class FileIndex {
 public:
  // Indexa recursivamente `dir` (caminho absoluto, separadores `\`).
  // Devolve quantos arquivos foram adicionados.
  size_t AddTree(const std::string& dir);

  // `path` completo; a comparacao ignora maiusculas (como o Windows).
  bool Contains(const std::string& path) const;

  size_t size() const { return files_.size(); }

 private:
  std::unordered_set<std::string> files_;  // caminhos completos em minusculas
};

}  // namespace aeonian

#endif  // AEONIAN_HOST_FILE_INDEX_H_
