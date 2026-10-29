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

#include "core/cmdline.h"

#include "core/ini.h"

namespace aeonian {

namespace {

bool HasToken(std::string_view line, std::string_view token) {
  std::string wanted = ToLowerAscii(token);
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    size_t start = i;
    while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
    if (i > start && ToLowerAscii(line.substr(start, i - start)) == wanted) {
      return true;
    }
  }
  return false;
}

}  // namespace

namespace {

// Remove de `line` todo token igual a `token` (inclusive o espaco que o
// antecede). Aspas nao sao interpretadas: o caminho do executavel vem entre
// aspas e nunca coincide com uma flag.
std::string RemoveToken(const std::string& line, std::string_view token) {
  const std::string wanted = ToLowerAscii(token);
  std::string out;
  size_t i = 0;
  while (i < line.size()) {
    size_t sep = i;
    while (sep < line.size() && (line[sep] == ' ' || line[sep] == '\t')) ++sep;
    size_t end = sep;
    while (end < line.size() && line[end] != ' ' && line[end] != '\t') ++end;
    if (end > sep && ToLowerAscii(std::string_view(line).substr(sep, end - sep)) == wanted) {
      i = end;  // descarta o separador e o token
    } else {
      out.append(line, i, end - i);
      i = end;
    }
  }
  return out;
}

}  // namespace

std::string BuildCommandLine(std::string_view original,
                             const std::vector<std::string>& extra,
                             const std::vector<std::string>& remove) {
  std::string out(original);
  for (const std::string& flag : remove) out = RemoveToken(out, flag);
  for (const std::string& flag : extra) {
    if (flag.empty() || HasToken(out, flag)) continue;
    // `remove` garante a AUSENCIA da flag, inclusive se ela veio de `extra`.
    bool forbidden = false;
    for (const std::string& r : remove) forbidden = forbidden || ToLowerAscii(r) == ToLowerAscii(flag);
    if (forbidden) continue;
    if (!out.empty() && out.back() != ' ') out += ' ';
    out += flag;
  }
  return out;
}

}  // namespace aeonian
