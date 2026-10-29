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

#include "host/file_index.h"

#include <windows.h>

#include <cstring>
#include <vector>

#include "core/ini.h"

namespace aeonian {

size_t FileIndex::AddTree(const std::string& dir) {
  size_t added = 0;
  std::vector<std::string> pending;
  pending.push_back(dir);
  while (!pending.empty()) {
    const std::string current = std::move(pending.back());
    pending.pop_back();
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((current + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) continue;
    do {
      if (std::strcmp(fd.cFileName, ".") == 0 ||
          std::strcmp(fd.cFileName, "..") == 0) {
        continue;
      }
      std::string full = current + "\\" + fd.cFileName;
      if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        pending.push_back(std::move(full));
      } else if (files_.insert(ToLowerAscii(full)).second) {
        ++added;
      }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
  }
  return added;
}

bool FileIndex::Contains(const std::string& path) const {
  return files_.count(ToLowerAscii(path)) != 0;
}

}  // namespace aeonian
