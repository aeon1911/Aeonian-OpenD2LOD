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

#include "host/media.h"

#include <string>

#include "host/inline_hook.h"
#include "host/log.h"
#include "host/runtime.h"

namespace aeonian {

BinkOpenFn g_real_bink_open = nullptr;
SmackOpenFn g_real_smack_open = nullptr;

namespace {

constexpr uint32_t kBinkFromMemory = 0x04000000u;  // 1o argumento nao e um nome
constexpr uint32_t kBinkFileHandle = 0x00800000u;  // 1o argumento e um HANDLE de arquivo

// Le o nome de forma segura (o ponteiro vem do jogo).
std::string SafeName(const char* name) {
  if (name == nullptr || !IsReadable(reinterpret_cast<uintptr_t>(name), 1)) return std::string();
  std::string out;
  for (int i = 0; i < 260; ++i) {
    if (!IsReadable(reinterpret_cast<uintptr_t>(name + i), 1)) break;
    const char c = name[i];
    if (c == '\0') return out;
    if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) > 0x7E) {
      return std::string();  // nao e texto: provavelmente um buffer de memoria
    }
    out += c;
  }
  return std::string();
}

}  // namespace

// Caminho do arquivo de um handle (para diagnostico).
std::string HandlePath(const void* handle) {
  char buf[MAX_PATH * 2];
  const DWORD n = GetFinalPathNameByHandleA(const_cast<HANDLE>(handle), buf, sizeof(buf), 0);
  return n > 0 && n < sizeof(buf) ? std::string(buf, n) : std::string();
}

void* WINAPI HookBinkOpen(const char* name, uint32_t flags) {
  Runtime& rt = Runtime::Get();
  rt.EnsureInitialized();
  const bool by_handle = (flags & kBinkFileHandle) != 0;
  const bool by_memory = (flags & kBinkFromMemory) != 0;
  std::string text = by_handle || by_memory ? std::string() : SafeName(name);
  bool skip = false;
  if (!text.empty()) {
    skip = rt.ShouldSkipVideo(text);
  } else {
    skip = rt.ShouldSkipIntroVideo();  // sem nome: so a regra dos videos de abertura
    if (by_handle) text = "handle " + HandlePath(name);
  }
  log::Writef(log::Level::kInfo, "video Bink: '%s' flags=0x%08x%s",
              text.empty() ? "(memoria)" : text.c_str(), flags, skip ? " -> PULADO" : "");
  if (skip) return nullptr;
  return g_real_bink_open(name, flags);
}

void* WINAPI HookSmackOpen(const char* name, uint32_t size_flags, uint32_t extra) {
  Runtime& rt = Runtime::Get();
  rt.EnsureInitialized();
  const std::string text = SafeName(name);
  const bool skip = text.empty() ? rt.ShouldSkipIntroVideo() : rt.ShouldSkipVideo(text);
  log::Writef(log::Level::kInfo, "video Smacker: '%s' flags=0x%08x%s",
              text.empty() ? "(memoria)" : text.c_str(), size_flags, skip ? " -> PULADO" : "");
  if (skip) return nullptr;
  return g_real_smack_open(name, size_flags, extra);
}

}  // namespace aeonian
