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

#include "stash_storage.h"

#include <cassert>
#include <iostream>

using namespace plugy_stash;

#define CHECK(expr)                                                        \
  do {                                                                     \
    if (!(expr)) {                                                         \
      std::cerr << "FALHA: " #expr " em " << __FILE__ << ":" << __LINE__   \
                << std::endl;                                              \
      return 1;                                                            \
    }                                                                      \
  } while (0)

int main() {
  std::cout << "[TEST] Iniciando testes de armazenamento PlugY (stash_storage)..." << std::endl;

  // 1. Inicialização padrão
  {
    StashStorage storage;
    CHECK(storage.IsSharedMode() == true);
    CHECK(storage.PageCount() == 1);
    CHECK(storage.CurrentPageIndex() == 0);
    CHECK(storage.CurrentPage() != nullptr);
    CHECK(storage.CurrentPage()->name == "Compartilhado 1");

    storage.SetSharedMode(false);
    CHECK(storage.IsSharedMode() == false);
    CHECK(storage.PageCount() == 1);
    CHECK(storage.CurrentPage()->name == "Pessoal 1");
  }

  // 2. Navegação de páginas e adição
  {
    StashStorage storage;
    storage.AddPage("Gemas");
    storage.AddPage("Runas");
    CHECK(storage.PageCount() == 3);
    CHECK(storage.CurrentPageIndex() == 2);
    CHECK(storage.CurrentPage()->name == "Runas");

    CHECK(storage.FirstPage());
    CHECK(storage.CurrentPageIndex() == 0);
    CHECK(!storage.PrevPage());

    CHECK(storage.NextPage());
    CHECK(storage.CurrentPageIndex() == 1);
    CHECK(storage.CurrentPage()->name == "Gemas");

    CHECK(storage.LastPage());
    CHECK(storage.CurrentPageIndex() == 2);
    CHECK(!storage.NextPage());

    CHECK(storage.GoToPage(1));
    CHECK(storage.CurrentPageIndex() == 1);

    // Remoção de página
    CHECK(storage.RemoveCurrentPage());
    CHECK(storage.PageCount() == 2);
    CHECK(storage.CurrentPageIndex() == 1);
    CHECK(storage.CurrentPage()->name == "Runas");

    CHECK(storage.RemoveCurrentPage());
    CHECK(storage.PageCount() == 1);
    // Não pode remover a última página restante
    CHECK(!storage.RemoveCurrentPage());
    CHECK(storage.PageCount() == 1);
  }

  // 3. Serialização e Deserialização de Baú Compartilhado (.sss)
  {
    std::vector<StashPage> pages;

    StashPage p1;
    p1.name = "Armas Unicas";
    p1.flags = 0x01;
    StashItem item1;
    item1.raw_data = {0x4A, 0x4D, 0x10, 0x20, 0x30, 0x40};  // 'JM' + payload
    p1.items.push_back(item1);
    pages.push_back(p1);

    StashPage p2;
    p2.name = "Runas Altas";
    p2.flags = 0x01;
    StashItem item2;
    item2.raw_data = {0x4A, 0x4D, 0xAA, 0xBB, 0xCC};
    p2.items.push_back(item2);
    pages.push_back(p2);

    const uint32_t gold_in = 2500000;
    std::vector<uint8_t> bytes = StashStorage::SerializeFile(pages, gold_in, false);
    CHECK(!bytes.empty());

    // Validar deserialização
    std::vector<StashPage> out_pages;
    uint32_t gold_out = 0;
    CHECK(StashStorage::ParseFile(bytes.data(), bytes.size(), false, &out_pages, &gold_out));
    CHECK(gold_out == gold_in);
    CHECK(out_pages.size() == 2);
    CHECK(out_pages[0].name == "Armas Unicas");
    CHECK(out_pages[0].items.size() == 1);
    CHECK(out_pages[0].items[0].raw_data == item1.raw_data);
    CHECK(out_pages[1].name == "Runas Altas");
    CHECK(out_pages[1].items.size() == 1);
    CHECK(out_pages[1].items[0].raw_data == item2.raw_data);
  }

  // 4. Serialização e Deserialização de Baú Pessoal (.d2x)
  {
    std::vector<StashPage> pages;
    StashPage p;
    p.name = "Pessoal Teste";
    p.flags = 0x00;
    pages.push_back(p);

    std::vector<uint8_t> bytes = StashStorage::SerializeFile(pages, 0, true);
    CHECK(!bytes.empty());

    std::vector<StashPage> out_pages;
    uint32_t gold_out = 0;
    // Tentar como compartilhado deve falhar pelo magic ("CSTM" != "SSS\0")
    CHECK(!StashStorage::ParseFile(bytes.data(), bytes.size(), false, &out_pages, &gold_out));
    // Como pessoal deve ter sucesso
    CHECK(StashStorage::ParseFile(bytes.data(), bytes.size(), true, &out_pages, &gold_out));
    CHECK(out_pages.size() == 1);
    CHECK(out_pages[0].name == "Pessoal Teste");
  }

  // 5. Dados inválidos e truncados
  {
    std::vector<StashPage> out_pages;
    uint32_t gold = 0;
    CHECK(!StashStorage::ParseFile(nullptr, 0, false, &out_pages, &gold));

    uint8_t short_buf[10] = {0};
    CHECK(!StashStorage::ParseFile(short_buf, sizeof(short_buf), false, &out_pages, &gold));

    uint8_t bad_magic[16] = {'B', 'A', 'D', '!', 0x31, 0x30, 0, 0, 0, 0, 1, 0, 0, 0};
    CHECK(!StashStorage::ParseFile(bad_magic, sizeof(bad_magic), false, &out_pages, &gold));
  }

  std::cout << "[TEST] Todos os testes de armazenamento PlugY passaram com sucesso!" << std::endl;
  return 0;
}
