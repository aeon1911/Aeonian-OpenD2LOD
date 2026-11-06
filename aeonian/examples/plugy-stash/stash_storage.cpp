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

#include <cstring>
#include <fstream>
#include <sstream>

namespace plugy_stash {

namespace {

uint16_t ReadU16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t ReadU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void WriteU16(std::vector<uint8_t>* out, uint16_t v) {
  out->push_back(static_cast<uint8_t>(v & 0xFFu));
  out->push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}

void WriteU32(std::vector<uint8_t>* out, uint32_t v) {
  out->push_back(static_cast<uint8_t>(v & 0xFFu));
  out->push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
  out->push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
  out->push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

}  // namespace

StashStorage::StashStorage() {
  // Inicializa com ao menos 1 página compartilhada e 1 página pessoal vazias
  StashPage default_shared;
  default_shared.name = "Compartilhado 1";
  default_shared.flags = 0x01;
  shared_pages_.push_back(std::move(default_shared));

  StashPage default_personal;
  default_personal.name = "Pessoal 1";
  default_personal.flags = 0x00;
  personal_pages_.push_back(std::move(default_personal));
}

bool StashStorage::ParseFile(const uint8_t* data, size_t size, bool is_personal,
                            std::vector<StashPage>* out_pages, uint32_t* out_gold) {
  if (data == nullptr || size < 14 || out_pages == nullptr) return false;

  const uint32_t magic = ReadU32(data);
  const uint32_t expected_magic = is_personal ? kPersonalMagic : kSharedMagic;
  if (magic != expected_magic) return false;

  const uint16_t version = ReadU16(data + 4);
  if (version != kVersion01 && version != kVersion02) return false;

  const uint32_t gold = ReadU32(data + 6);
  if (out_gold != nullptr) *out_gold = gold;

  const uint32_t pages_count = ReadU32(data + 10);
  size_t offset = 14;

  std::vector<StashPage> pages;
  pages.reserve(pages_count);

  for (uint32_t p = 0; p < pages_count && offset + 6 <= size; ++p) {
    StashPage page;
    const uint16_t st_header = ReadU16(data + offset);
    if (st_header != kPageHeader) {
      break;
    }
    offset += 2;

    page.flags = ReadU32(data + offset);
    offset += 4;

    // Nome da página (string terminada em \0, até 64 bytes)
    std::string name;
    while (offset < size && data[offset] != 0 && name.size() < 64) {
      name.push_back(static_cast<char>(data[offset]));
      ++offset;
    }
    if (offset < size && data[offset] == 0) {
      ++offset;
    }
    page.name = name.empty() ? ("Pagina " + std::to_string(p + 1)) : name;

    // Cabeçalho do bloco de itens: "JM"
    if (offset + 4 <= size) {
      const uint16_t jm_header = ReadU16(data + offset);
      if (jm_header == kItemHeader) {
        offset += 2;
        const uint16_t item_count = ReadU16(data + offset);
        offset += 2;

        for (uint16_t i = 0; i < item_count && offset + 4 <= size; ++i) {
          // Cada item inicia com 'JM' ou bitstream
          const uint16_t item_jm = ReadU16(data + offset);
          if (item_jm != kItemHeader) {
            // Em caso de corrupção ou fim antecipado
            break;
          }
          offset += 2;

          // Localiza o fim deste item:
          // Se for o ultimo item da pagina (i + 1 == item_count), vai ate o proximo 'ST' ou fim do buffer.
          // Se houver outro item (i + 1 < item_count), vai ate o proximo 'JM'.
          size_t item_end = offset;
          while (item_end + 1 < size) {
            const uint16_t sig = ReadU16(data + item_end);
            if (i + 1 < item_count && sig == kItemHeader) {
              break;
            }
            if (sig == kPageHeader) {
              break;
            }
            ++item_end;
          }
          if (item_end + 1 >= size) {
            item_end = size;
          }

          StashItem item;
          item.raw_data.reserve((item_end - offset) + 2);
          item.raw_data.push_back(0x4A);  // 'J'
          item.raw_data.push_back(0x4D);  // 'M'
          item.raw_data.insert(item.raw_data.end(), data + offset, data + item_end);
          page.items.push_back(std::move(item));

          offset = item_end;
        }
      }
    }

    pages.push_back(std::move(page));
  }

  if (pages.empty()) return false;
  *out_pages = std::move(pages);
  return true;
}

std::vector<uint8_t> StashStorage::SerializeFile(const std::vector<StashPage>& pages,
                                                 uint32_t gold, bool is_personal) {
  std::vector<uint8_t> out;
  out.reserve(256);

  // 1. Cabeçalho
  WriteU32(&out, is_personal ? kPersonalMagic : kSharedMagic);
  WriteU16(&out, kVersion02);
  WriteU32(&out, gold);
  WriteU32(&out, static_cast<uint32_t>(pages.size()));

  // 2. Páginas
  for (const auto& page : pages) {
    WriteU16(&out, kPageHeader);  // "ST"
    WriteU32(&out, page.flags);

    // Nome da página + \0
    for (char c : page.name) {
      out.push_back(static_cast<uint8_t>(c));
    }
    out.push_back(0);

    // Bloco de itens: "JM" + itemCount
    WriteU16(&out, kItemHeader);  // "JM"
    WriteU16(&out, static_cast<uint16_t>(page.items.size()));

    // Itens
    for (const auto& item : page.items) {
      if (item.raw_data.size() >= 2 && item.raw_data[0] == 0x4A &&
          item.raw_data[1] == 0x4D) {
        out.insert(out.end(), item.raw_data.begin(), item.raw_data.end());
      } else {
        // Assegura prefixo 'JM'
        WriteU16(&out, kItemHeader);
        out.insert(out.end(), item.raw_data.begin(), item.raw_data.end());
      }
    }
  }

  return out;
}

bool StashStorage::LoadShared(const std::string& filepath) {
  std::ifstream in(filepath, std::ios::binary);
  if (!in.is_open()) return false;

  std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
  std::vector<StashPage> pages;
  uint32_t gold = 0;
  if (!ParseFile(buffer.data(), buffer.size(), false, &pages, &gold)) {
    return false;
  }
  shared_pages_ = std::move(pages);
  shared_gold_ = gold;
  if (current_shared_page_ >= shared_pages_.size()) {
    current_shared_page_ = 0;
  }
  return true;
}

bool StashStorage::SaveShared(const std::string& filepath) const {
  std::vector<uint8_t> data = SerializeFile(shared_pages_, shared_gold_, false);
  std::ofstream out(filepath, std::ios::binary);
  if (!out.is_open()) return false;
  out.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
  return out.good();
}

bool StashStorage::LoadPersonal(const std::string& filepath) {
  std::ifstream in(filepath, std::ios::binary);
  if (!in.is_open()) return false;

  std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
  std::vector<StashPage> pages;
  uint32_t gold = 0;
  if (!ParseFile(buffer.data(), buffer.size(), true, &pages, &gold)) {
    return false;
  }
  personal_pages_ = std::move(pages);
  if (current_personal_page_ >= personal_pages_.size()) {
    current_personal_page_ = 0;
  }
  return true;
}

bool StashStorage::SavePersonal(const std::string& filepath) const {
  std::vector<uint8_t> data = SerializeFile(personal_pages_, 0, true);
  std::ofstream out(filepath, std::ios::binary);
  if (!out.is_open()) return false;
  out.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
  return out.good();
}

StashPage* StashStorage::CurrentPage() {
  auto& pages = is_shared_mode_ ? shared_pages_ : personal_pages_;
  size_t idx = is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  if (pages.empty()) return nullptr;
  if (idx >= pages.size()) idx = 0;
  return &pages[idx];
}

const StashPage* StashStorage::CurrentPage() const {
  const auto& pages = is_shared_mode_ ? shared_pages_ : personal_pages_;
  size_t idx = is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  if (pages.empty()) return nullptr;
  if (idx >= pages.size()) idx = 0;
  return &pages[idx];
}

bool StashStorage::NextPage() {
  const size_t total = PageCount();
  if (total <= 1) return false;
  size_t& cur = is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  if (cur + 1 < total) {
    ++cur;
    return true;
  }
  return false;
}

bool StashStorage::PrevPage() {
  size_t& cur = is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  if (cur > 0) {
    --cur;
    return true;
  }
  return false;
}

bool StashStorage::FirstPage() {
  size_t& cur = is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  if (cur != 0) {
    cur = 0;
    return true;
  }
  return false;
}

bool StashStorage::LastPage() {
  const size_t total = PageCount();
  if (total == 0) return false;
  size_t& cur = is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  if (cur != total - 1) {
    cur = total - 1;
    return true;
  }
  return false;
}

bool StashStorage::GoToPage(size_t index) {
  if (index >= PageCount()) return false;
  size_t& cur = is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  cur = index;
  return true;
}

void StashStorage::AddPage(const std::string& name) {
  auto& pages = is_shared_mode_ ? shared_pages_ : personal_pages_;
  StashPage p;
  p.name = name.empty() ? ("Pagina " + std::to_string(pages.size() + 1)) : name;
  p.flags = is_shared_mode_ ? 0x01 : 0x00;
  pages.push_back(std::move(p));
  GoToPage(pages.size() - 1);
}

bool StashStorage::RemoveCurrentPage() {
  auto& pages = is_shared_mode_ ? shared_pages_ : personal_pages_;
  if (pages.size() <= 1) return false;  // Mantém sempre ao menos 1 página
  size_t& cur = is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  pages.erase(pages.begin() + static_cast<std::ptrdiff_t>(cur));
  if (cur >= pages.size()) {
    cur = pages.size() - 1;
  }
  return true;
}

}  // namespace plugy_stash
