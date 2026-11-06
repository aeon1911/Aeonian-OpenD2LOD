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

#ifndef AEONIAN_EXAMPLES_PLUGY_STASH_STASH_STORAGE_H_
#define AEONIAN_EXAMPLES_PLUGY_STASH_STASH_STORAGE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace plugy_stash {

// Constantes de formato PlugY (.sss e .d2x).
// Ver docs/specs/game-exe-1.14d-inventory-stash.md sec. 5.
inline constexpr uint32_t kSharedMagic = 0x00535353u;    // "SSS\0"
inline constexpr uint32_t kPersonalMagic = 0x4D545343u;  // "CSTM"
inline constexpr uint16_t kVersion01 = 0x3130u;          // "01"
inline constexpr uint16_t kVersion02 = 0x3230u;          // "02"
inline constexpr uint16_t kPageHeader = 0x5453u;         // "ST"
inline constexpr uint16_t kItemHeader = 0x4D4Au;         // "JM"

struct StashItem {
  std::vector<uint8_t> raw_data;  // Bitstream binario do item (inicia com 'JM')
  uint8_t col = 0;                // Coluna X (0..9)
  uint8_t row = 0;                // Linha Y (0..9)
  uint8_t page = 4;               // Codigo de contêiner (0x04 = Stash)
};

struct StashPage {
  std::string name;
  uint32_t flags = 0;  // 0x01 = compartilhada, 0x00 = pessoal
  std::vector<StashItem> items;
};

class StashStorage {
 public:
  StashStorage();

  // Carrega e salva baú compartilhado (.sss) e pessoal (.d2x)
  bool LoadShared(const std::string& filepath);
  bool SaveShared(const std::string& filepath) const;
  bool LoadPersonal(const std::string& filepath);
  bool SavePersonal(const std::string& filepath) const;

  // Serialização/Deserialização pura em memória (portável, testável no host)
  static bool ParseFile(const uint8_t* data, size_t size, bool is_personal,
                        std::vector<StashPage>* out_pages, uint32_t* out_gold);
  static std::vector<uint8_t> SerializeFile(const std::vector<StashPage>& pages,
                                            uint32_t gold, bool is_personal);

  // Gestão de páginas
  bool IsSharedMode() const { return is_shared_mode_; }
  void SetSharedMode(bool shared) { is_shared_mode_ = shared; }

  size_t CurrentPageIndex() const {
    return is_shared_mode_ ? current_shared_page_ : current_personal_page_;
  }
  size_t PageCount() const {
    return is_shared_mode_ ? shared_pages_.size() : personal_pages_.size();
  }

  StashPage* CurrentPage();
  const StashPage* CurrentPage() const;

  bool NextPage();
  bool PrevPage();
  bool FirstPage();
  bool LastPage();
  bool GoToPage(size_t index);

  void AddPage(const std::string& name);
  bool RemoveCurrentPage();

  uint32_t SharedGold() const { return shared_gold_; }
  void SetSharedGold(uint32_t gold) { shared_gold_ = gold; }

 private:
  uint32_t shared_gold_ = 0;
  std::vector<StashPage> shared_pages_;
  std::vector<StashPage> personal_pages_;
  size_t current_shared_page_ = 0;
  size_t current_personal_page_ = 0;
  bool is_shared_mode_ = true;
};

}  // namespace plugy_stash

#endif  // AEONIAN_EXAMPLES_PLUGY_STASH_STASH_STORAGE_H_
