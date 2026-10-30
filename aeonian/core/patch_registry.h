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

// aeonian/core/patch_registry.h
//
// Registro das faixas de memoria que o PROPRIO motor alterou (hook_inline e
// patch_bytes), com os bytes que estavam la antes da primeira escrita. Serve
// para:
//   * resolve_symbol conferir o banco de enderecos contra os bytes ORIGINAIS
//     de uma funcao que um mod ja desviou;
//   * read_original devolver aos mods os bytes da imagem original (ancoras
//     de dados dentro de uma funcao desviada);
//   * decidir o que fazer com uma escrita nova sobre uma faixa ja alterada
//     (encadear o hook, aceitar o patch do mesmo mod ou recusar).
//
// Logica pura (sem Win32): a memoria entra e sai por buffers, o que permite
// testar no host. O chamador serializa o acesso.

#ifndef AEONIAN_CORE_PATCH_REGISTRY_H_
#define AEONIAN_CORE_PATCH_REGISTRY_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace aeonian {

enum class PatchKind { kHook, kBytes };

struct PatchRange {
  uintptr_t address = 0;
  PatchKind kind = PatchKind::kBytes;
  std::string owners;             // mods, na ordem em que escreveram ("a > b")
  std::vector<uint8_t> original;  // antes da 1a escrita do motor
  std::vector<uint8_t> written;   // o que o motor deixou la
  uintptr_t detour = 0;           // kHook: destino atual do desvio

  uintptr_t end() const { return address + original.size(); }
};

// O que fazer com uma escrita nova sobre [address, address+length).
enum class WriteVerdict {
  kFree,      // nenhuma faixa registrada no caminho
  kChain,     // hook no MESMO inicio de um hook registrado: encadear
  kOwnBytes,  // patch que so cruza patches do mesmo mod
  kConflict,  // qualquer outra sobreposicao: recusar
};

class PatchRegistry {
 public:
  // `match` (opcional) recebe a faixa que levou a kChain/kConflict. O
  // ponteiro vale ate a proxima alteracao do registro.
  WriteVerdict Check(uintptr_t address, size_t length, PatchKind kind,
                     std::string_view owner,
                     const PatchRange** match = nullptr) const;

  // `bytes` chega com a memoria ATUAL de [address, address+length) e sai com
  // ela como estava antes de o motor escrever: as faixas registradas voltam
  // aos originais. Onde a memoria atual ja nao e o que o motor escreveu
  // (alguem de fora mexeu), fica o byte atual e a conferencia do chamador
  // falha, como deve. Devolve quantos bytes foram trocados; `owners`
  // (opcional) recebe os mods das faixas usadas.
  size_t RestoreOriginal(uintptr_t address, uint8_t* bytes, size_t length,
                         std::string* owners = nullptr) const;

  // Registra uma escrita ja feita. `before`/`after`: conteudo de
  // [address, address+length) antes e depois dela. Faixas do mesmo tipo e
  // mod que se sobrepoem viram uma so; um patch que voltou ao original sai
  // do registro.
  void Record(uintptr_t address, const uint8_t* before, const uint8_t* after,
              size_t length, PatchKind kind, std::string_view owner,
              uintptr_t detour = 0);

  // Hook encadeado no inicio de um hook registrado: o desvio em `address`
  // agora e `jump` e leva a `detour`.
  void Rechain(uintptr_t address, const uint8_t* jump, size_t jump_length,
               uintptr_t detour, std::string_view owner);

  const std::vector<PatchRange>& ranges() const { return ranges_; }

 private:
  std::vector<PatchRange> ranges_;  // disjuntas, em ordem de endereco
};

}  // namespace aeonian

#endif  // AEONIAN_CORE_PATCH_REGISTRY_H_
