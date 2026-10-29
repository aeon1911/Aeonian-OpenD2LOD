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

// aeonian/host/win_util.h
//
// Utilitarios Win32 pequenos. Todo caminho e tratado na pagina de codigo ANSI
// (as mesmas APIs *A que o proprio Game.exe usa), para que os caminhos que
// devolvemos ao jogo sejam consistentes com os que ele nos entrega.

#ifndef AEONIAN_HOST_WIN_UTIL_H_
#define AEONIAN_HOST_WIN_UTIL_H_

#include <windows.h>

#include <cstring>
#include <string>
#include <vector>

namespace aeonian {

inline std::string DirName(const std::string& path) {
  size_t cut = path.find_last_of("\\/");
  return cut == std::string::npos ? std::string() : path.substr(0, cut);
}

inline std::string ModulePath(HMODULE module) {
  char buf[MAX_PATH * 2];
  DWORD n = GetModuleFileNameA(module, buf, sizeof(buf));
  if (n == 0 || n >= sizeof(buf)) return std::string();
  return std::string(buf, n);
}

inline bool FileExists(const std::string& path) {
  DWORD a = GetFileAttributesA(path.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

inline bool DirExists(const std::string& path) {
  DWORD a = GetFileAttributesA(path.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

inline bool PathExists(const std::string& path) {
  return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

inline bool ReadWholeFile(const std::string& path, std::string* out) {
  HANDLE h = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return false;
  LARGE_INTEGER size;
  bool ok = GetFileSizeEx(h, &size) && size.QuadPart >= 0 &&
            size.QuadPart < (16 << 20);  // manifestos/INI: 16 MiB e o teto
  if (ok) {
    out->assign(static_cast<size_t>(size.QuadPart), '\0');
    size_t done = 0;
    while (ok && done < out->size()) {
      DWORD got = 0;
      ok = ReadFile(h, &(*out)[done], static_cast<DWORD>(out->size() - done),
                    &got, nullptr) &&
           got != 0;
      done += got;
    }
    ok = ok && done == out->size();
  }
  CloseHandle(h);
  return ok;
}

// Nomes (sem caminho) das entradas de `dir` que casam com `pattern`.
inline std::vector<std::string> ListDir(const std::string& dir,
                                        const char* pattern,
                                        bool want_directories) {
  std::vector<std::string> names;
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA((dir + "\\" + pattern).c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return names;
  do {
    bool is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (is_dir != want_directories) continue;
    if (std::strcmp(fd.cFileName, ".") == 0 ||
        std::strcmp(fd.cFileName, "..") == 0) {
      continue;
    }
    names.emplace_back(fd.cFileName);
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  return names;
}

// GetProcAddress com o cast para o tipo de funcao desejado.
template <typename Fn>
Fn ProcAddress(HMODULE module, const char* name) {
  FARPROC raw = GetProcAddress(module, name);
  Fn fn = nullptr;
  static_assert(sizeof(fn) == sizeof(raw), "ponteiro de funcao inesperado");
  std::memcpy(&fn, &raw, sizeof(fn));
  return fn;
}

}  // namespace aeonian

#endif  // AEONIAN_HOST_WIN_UTIL_H_
