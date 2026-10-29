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

#include "core/address_db.h"

#include "core/ini.h"

namespace aeonian {

namespace {

constexpr std::string_view kSymbolPrefix = "symbol.";

int HexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace

std::optional<std::vector<uint8_t>> ParseHexBytes(std::string_view text) {
  std::vector<uint8_t> out;
  int high = -1;
  for (char c : text) {
    if (c == ' ' || c == '\t' || c == ',') {
      if (high != -1) return std::nullopt;  // nibble solto
      continue;
    }
    int v = HexValue(c);
    if (v < 0) return std::nullopt;
    if (high == -1) {
      high = v;
    } else {
      out.push_back(static_cast<uint8_t>(high * 16 + v));
      high = -1;
    }
  }
  if (high != -1) return std::nullopt;
  return out;
}

std::optional<AddressDb> AddressDb::Parse(std::string_view text,
                                          std::string* error) {
  auto fail = [&](std::string message) -> std::optional<AddressDb> {
    if (error != nullptr) *error = std::move(message);
    return std::nullopt;
  };

  Ini ini = Ini::Parse(text);
  if (!ini.HasSection("build")) return fail("secao [build] ausente");

  AddressDb db;
  db.build_name_ = ini.GetOr("build", "name", "");
  if (db.build_name_.empty()) return fail("[build] name ausente");
  db.build_id_ = static_cast<uint32_t>(ini.GetInt("build", "id", 0));
  db.sha256_ = ToLowerAscii(ini.GetOr("build", "sha256", ""));
  int64_t timestamp = ini.GetInt("build", "timestamp", -1);
  int64_t size_of_image = ini.GetInt("build", "size_of_image", -1);
  int64_t entry_rva = ini.GetInt("build", "entry_rva", -1);
  if (timestamp < 0 || size_of_image < 0 || entry_rva < 0) {
    return fail("[build] exige timestamp, size_of_image e entry_rva");
  }
  db.timestamp_ = static_cast<uint32_t>(timestamp);
  db.size_of_image_ = static_cast<uint32_t>(size_of_image);
  db.entry_rva_ = static_cast<uint32_t>(entry_rva);

  for (const std::string& section : ini.SectionNames()) {
    std::string lower = ToLowerAscii(section);
    if (lower.compare(0, kSymbolPrefix.size(), kSymbolPrefix) != 0) continue;
    Symbol sym;
    sym.name = section.substr(kSymbolPrefix.size());
    if (sym.name.empty()) return fail("simbolo sem nome");
    int64_t rva = ini.GetInt(section, "rva", -1);
    if (rva < 0) return fail("simbolo '" + sym.name + "' sem rva valido");
    sym.rva = static_cast<uint32_t>(rva);
    if (std::optional<std::string> bytes = ini.Get(section, "bytes")) {
      std::optional<std::vector<uint8_t>> parsed = ParseHexBytes(*bytes);
      if (!parsed) return fail("simbolo '" + sym.name + "': bytes invalidos");
      sym.expect = std::move(*parsed);
    }
    sym.description = ini.GetOr(section, "description", "");
    db.symbols_.push_back(std::move(sym));
  }
  return db;
}

bool AddressDb::Matches(const PeInfo& pe) const {
  return pe.timestamp == timestamp_ && pe.size_of_image == size_of_image_ &&
         pe.entry_rva == entry_rva_;
}

const Symbol* AddressDb::Find(std::string_view name) const {
  std::string wanted = ToLowerAscii(name);
  for (const Symbol& s : symbols_) {
    if (ToLowerAscii(s.name) == wanted) return &s;
  }
  return nullptr;
}

}  // namespace aeonian
