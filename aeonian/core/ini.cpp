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

#include "core/ini.h"

#include <cstddef>

namespace aeonian {

namespace {

bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

}  // namespace

std::string ToLowerAscii(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

std::string_view Trim(std::string_view text) {
  size_t begin = 0;
  size_t end = text.size();
  while (begin < end && IsSpace(text[begin])) ++begin;
  while (end > begin && IsSpace(text[end - 1])) --end;
  return text.substr(begin, end - begin);
}

std::optional<int64_t> ParseInt(std::string_view text) {
  text = Trim(text);
  if (text.empty()) return std::nullopt;
  bool negative = false;
  if (text.front() == '-' || text.front() == '+') {
    negative = text.front() == '-';
    text.remove_prefix(1);
  }
  int base = 10;
  if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
    base = 16;
    text.remove_prefix(2);
  }
  if (text.empty()) return std::nullopt;
  int64_t value = 0;
  for (char c : text) {
    int digit;
    if (c >= '0' && c <= '9') {
      digit = c - '0';
    } else if (base == 16 && c >= 'a' && c <= 'f') {
      digit = c - 'a' + 10;
    } else if (base == 16 && c >= 'A' && c <= 'F') {
      digit = c - 'A' + 10;
    } else {
      return std::nullopt;
    }
    if (value > (INT64_MAX - digit) / base) return std::nullopt;  // overflow
    value = value * base + digit;
  }
  return negative ? -value : value;
}

std::vector<std::string> SplitList(std::string_view text) {
  std::vector<std::string> out;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && (IsSpace(text[i]) || text[i] == ',')) ++i;
    size_t start = i;
    while (i < text.size() && !IsSpace(text[i]) && text[i] != ',') ++i;
    if (i > start) out.emplace_back(text.substr(start, i - start));
  }
  return out;
}

Ini::SectionData* Ini::FindOrAdd(std::string_view name) {
  std::string key = ToLowerAscii(name);
  for (SectionData& s : sections_) {
    if (s.key == key) return &s;
  }
  sections_.push_back(SectionData{key, {}});
  names_.emplace_back(name);
  return &sections_.back();
}

const Ini::SectionData* Ini::Find(std::string_view name) const {
  std::string key = ToLowerAscii(name);
  for (const SectionData& s : sections_) {
    if (s.key == key) return &s;
  }
  return nullptr;
}

Ini Ini::Parse(std::string_view text) {
  Ini ini;
  // BOM UTF-8.
  if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
      static_cast<unsigned char>(text[1]) == 0xBB &&
      static_cast<unsigned char>(text[2]) == 0xBF) {
    text.remove_prefix(3);
  }
  size_t current = static_cast<size_t>(-1);  // indice da secao atual
  auto select = [&](std::string_view name) {
    ini.FindOrAdd(name);
    std::string key = ToLowerAscii(name);
    for (size_t i = 0; i < ini.sections_.size(); ++i) {
      if (ini.sections_[i].key == key) current = i;
    }
  };

  size_t pos = 0;
  while (pos <= text.size()) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string_view::npos) eol = text.size();
    std::string_view line = Trim(text.substr(pos, eol - pos));
    pos = eol + 1;
    if (line.empty() || line.front() == ';' || line.front() == '#') continue;

    if (line.front() == '[') {
      size_t close = line.find(']');
      if (close == std::string_view::npos) continue;  // linha malformada
      select(Trim(line.substr(1, close - 1)));
      continue;
    }
    size_t eq = line.find('=');
    if (eq == std::string_view::npos) continue;
    std::string_view key = Trim(line.substr(0, eq));
    std::string_view value = Trim(line.substr(eq + 1));
    if (key.empty()) continue;

    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
      value = value.substr(1, value.size() - 2);
    } else {
      // comentario no fim da linha: " ;" ou " #"
      for (size_t i = 1; i < value.size(); ++i) {
        if ((value[i] == ';' || value[i] == '#') && IsSpace(value[i - 1])) {
          value = Trim(value.substr(0, i));
          break;
        }
      }
    }
    if (current == static_cast<size_t>(-1)) select("");
    ini.sections_[current].entries.emplace_back(ToLowerAscii(key),
                                                std::string(value));
  }
  return ini;
}

bool Ini::HasSection(std::string_view section) const {
  return Find(section) != nullptr;
}

std::optional<std::string> Ini::Get(std::string_view section,
                                    std::string_view key) const {
  const SectionData* s = Find(section);
  if (s == nullptr) return std::nullopt;
  std::string k = ToLowerAscii(key);
  for (size_t i = s->entries.size(); i > 0; --i) {  // a ultima ocorrencia vence
    if (s->entries[i - 1].first == k) return s->entries[i - 1].second;
  }
  return std::nullopt;
}

std::string Ini::GetOr(std::string_view section, std::string_view key,
                       std::string_view fallback) const {
  std::optional<std::string> v = Get(section, key);
  return v ? *v : std::string(fallback);
}

bool Ini::GetBool(std::string_view section, std::string_view key,
                  bool fallback) const {
  std::optional<std::string> v = Get(section, key);
  if (!v) return fallback;
  std::string s = ToLowerAscii(*v);
  if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
  if (s == "0" || s == "false" || s == "no" || s == "off") return false;
  return fallback;
}

int64_t Ini::GetInt(std::string_view section, std::string_view key,
                    int64_t fallback) const {
  std::optional<std::string> v = Get(section, key);
  if (!v) return fallback;
  std::optional<int64_t> n = ParseInt(*v);
  return n ? *n : fallback;
}

Ini::Entries Ini::Section(std::string_view section) const {
  const SectionData* s = Find(section);
  return s != nullptr ? s->entries : Entries{};
}

}  // namespace aeonian
