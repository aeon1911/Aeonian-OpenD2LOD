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

#include "core/vfs.h"

#include <utility>

#include "core/ini.h"

namespace aeonian {

namespace {

std::string ToBackslashes(std::string_view in) {
  std::string out(in);
  for (char& c : out) {
    if (c == '/') c = '\\';
  }
  return out;
}

bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
  if (text.size() < prefix.size()) return false;
  return ToLowerAscii(text.substr(0, prefix.size())) == ToLowerAscii(prefix);
}

bool IsAbsolute(std::string_view p) {
  if (p.size() >= 2 && p[1] == ':') return true;  // X:\...
  return p.size() >= 2 && p[0] == '\\' && p[1] == '\\';  // UNC / \\?\ / \\.\ .
}

}  // namespace

std::string NormalizeDir(std::string_view dir) {
  std::string out = ToBackslashes(dir);
  while (out.size() > 1 && out.back() == '\\') out.pop_back();
  return out;
}

Vfs::Vfs(std::string game_dir, std::vector<ModRoot> roots, ExistsFn exists)
    : game_dir_(NormalizeDir(game_dir)),
      roots_(std::move(roots)),
      exists_(std::move(exists)) {
  for (ModRoot& r : roots_) r.dir = NormalizeDir(r.dir);
}

std::optional<std::string> Vfs::RelativeDataPath(
    std::string_view requested) const {
  std::string p = ToBackslashes(requested);
  if (StartsWithNoCase(p, "\\\\?\\")) p.erase(0, 4);

  std::string rel;
  if (IsAbsolute(p)) {
    if (!StartsWithNoCase(p, game_dir_)) return std::nullopt;
    rel = p.substr(game_dir_.size());
    if (rel.empty() || rel.front() != '\\') return std::nullopt;  // C:\jogoX
  } else {
    rel = p;  // relativo: o jogo roda com a pasta atual = pasta do jogo
  }

  // Divide em segmentos descartando vazios e `.`; recusa `..` (um mod nunca
  // deve conseguir sair da propria pasta).
  std::vector<std::string_view> parts;
  size_t i = 0;
  while (i <= rel.size()) {
    size_t j = rel.find('\\', i);
    if (j == std::string::npos) j = rel.size();
    std::string_view seg(rel.data() + i, j - i);
    if (seg == "..") return std::nullopt;
    if (!seg.empty() && seg != ".") parts.push_back(seg);
    i = j + 1;
  }
  if (parts.size() < 2 || ToLowerAscii(parts[0]) != "data") return std::nullopt;

  std::string out;
  for (size_t k = 1; k < parts.size(); ++k) {
    if (k > 1) out += '\\';
    out.append(parts[k]);
  }
  // Curingas nunca correspondem a um arquivo concreto de mod.
  if (out.find_first_of("*?<>|\"") != std::string::npos) return std::nullopt;
  return out;
}

std::optional<std::string> Vfs::Resolve(std::string_view requested,
                                        std::string* owner) const {
  std::optional<std::string> rel = RelativeDataPath(requested);
  if (!rel) return std::nullopt;
  for (const ModRoot& root : roots_) {
    std::string candidate = root.dir + "\\data\\" + *rel;
    if (exists_(candidate)) {
      if (owner != nullptr) *owner = root.id;
      return candidate;
    }
  }
  return std::nullopt;
}

}  // namespace aeonian
