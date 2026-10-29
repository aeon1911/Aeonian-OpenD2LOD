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

#include "core/mod_manifest.h"

#include <algorithm>

#include "core/ini.h"

namespace aeonian {

namespace {

// Plugin e um caminho relativo dentro da pasta do mod, nunca absoluto nem
// com `..`.
bool IsSafeRelativePath(std::string_view p) {
  if (p.empty()) return false;
  if (p.front() == '/' || p.front() == '\\') return false;
  if (p.size() >= 2 && p[1] == ':') return false;
  size_t i = 0;
  while (i <= p.size()) {
    size_t j = p.find_first_of("/\\", i);
    if (j == std::string_view::npos) j = p.size();
    if (p.substr(i, j - i) == "..") return false;
    i = j + 1;
  }
  return true;
}

}  // namespace

bool IsValidModId(std::string_view id) {
  if (id.empty() || id.size() > 64) return false;
  for (char c : id) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
              c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

std::optional<ModManifest> ParseModManifest(std::string_view text,
                                            std::string_view dir_name,
                                            std::string* error) {
  auto fail = [&](std::string message) -> std::optional<ModManifest> {
    if (error != nullptr) *error = std::move(message);
    return std::nullopt;
  };

  Ini ini = Ini::Parse(text);
  ModManifest mod;
  mod.id = ToLowerAscii(ini.GetOr("mod", "id", dir_name));
  if (!IsValidModId(mod.id)) {
    return fail("id invalido '" + mod.id + "' (use [a-z0-9._-], ate 64)");
  }
  mod.name = ini.GetOr("mod", "name", mod.id);
  mod.version = ini.GetOr("mod", "version", "0.0.0");
  mod.author = ini.GetOr("mod", "author", "");

  int64_t priority = ini.GetInt("mod", "priority", 0);
  if (priority < -100000 || priority > 100000) {
    return fail("priority fora do intervalo [-100000, 100000]");
  }
  mod.priority = static_cast<int>(priority);

  for (const std::string& plugin :
       SplitList(ini.GetOr("mod", "plugin", ""))) {
    if (!IsSafeRelativePath(plugin)) {
      return fail("plugin '" + plugin + "' deve ser caminho relativo seguro");
    }
    mod.plugins.push_back(plugin);
  }
  mod.args = SplitList(ini.GetOr("mod", "args", ""));
  for (const std::string& a : mod.args) {
    if (a.empty() || a.front() != '-') {
      return fail("args: '" + a + "' nao e uma flag (comece com '-')");
    }
  }
  for (const std::string& b : SplitList(ini.GetOr("mod", "builds", ""))) {
    mod.builds.push_back(ToLowerAscii(b));
  }
  return mod;
}

void SortForLoad(std::vector<ModManifest>* mods) {
  std::sort(mods->begin(), mods->end(),
            [](const ModManifest& a, const ModManifest& b) {
              if (a.priority != b.priority) return a.priority > b.priority;
              return a.id < b.id;
            });
}

bool SupportsBuild(const ModManifest& mod, std::string_view build_name) {
  if (mod.builds.empty()) return true;
  std::string wanted = ToLowerAscii(build_name);
  return std::find(mod.builds.begin(), mod.builds.end(), wanted) !=
         mod.builds.end();
}

}  // namespace aeonian
