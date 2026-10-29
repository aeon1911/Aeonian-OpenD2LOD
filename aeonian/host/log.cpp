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

#include "host/log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

#include "core/ini.h"

namespace aeonian::log {

namespace {

CRITICAL_SECTION g_lock;
INIT_ONCE g_lock_once = INIT_ONCE_STATIC_INIT;
HANDLE g_file = INVALID_HANDLE_VALUE;
Level g_level = Level::kInfo;
DWORD g_start_ms = 0;

BOOL CALLBACK InitLock(PINIT_ONCE, PVOID, PVOID*) {
  InitializeCriticalSection(&g_lock);
  return TRUE;
}

void EnsureLock() { InitOnceExecuteOnce(&g_lock_once, InitLock, nullptr, nullptr); }

const char* Name(Level level) {
  switch (level) {
    case Level::kDebug: return "DEBUG";
    case Level::kInfo: return "INFO ";
    case Level::kWarn: return "WARN ";
    case Level::kError: return "ERROR";
  }
  return "?????";
}

}  // namespace

void Init(const std::string& path, Level min_level) {
  EnsureLock();
  EnterCriticalSection(&g_lock);
  g_level = min_level;
  if (g_file == INVALID_HANDLE_VALUE) {
    g_file = CreateFileA(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                         CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    g_start_ms = GetTickCount();
  }
  LeaveCriticalSection(&g_lock);
}

void SetLevel(Level level) { g_level = level; }
Level GetLevel() { return g_level; }

Level ParseLevel(std::string_view text, Level fallback) {
  std::string t = ToLowerAscii(text);
  if (t == "debug") return Level::kDebug;
  if (t == "info") return Level::kInfo;
  if (t == "warn" || t == "warning") return Level::kWarn;
  if (t == "error") return Level::kError;
  return fallback;
}

void Write(Level level, std::string_view message) {
  if (level < g_level) return;
  EnsureLock();
  char head[48];
  DWORD elapsed = GetTickCount() - g_start_ms;
  int n = std::snprintf(head, sizeof(head), "[%4lu.%03lu] [T%04lx] %s ",
                        static_cast<unsigned long>(elapsed / 1000),
                        static_cast<unsigned long>(elapsed % 1000),
                        static_cast<unsigned long>(GetCurrentThreadId()),
                        Name(level));
  std::string line(head, static_cast<size_t>(n > 0 ? n : 0));
  line.append(message);
  line += '\n';

  EnterCriticalSection(&g_lock);
  if (g_file != INVALID_HANDLE_VALUE) {
    DWORD written = 0;
    WriteFile(g_file, line.data(), static_cast<DWORD>(line.size()), &written,
              nullptr);
  } else {
    OutputDebugStringA(line.c_str());
  }
  LeaveCriticalSection(&g_lock);
}

void Writef(Level level, const char* format, ...) {
  if (level < g_level) return;
  char buf[1024];
  va_list args;
  va_start(args, format);
  int n = std::vsnprintf(buf, sizeof(buf), format, args);
  va_end(args);
  if (n < 0) return;
  size_t len = static_cast<size_t>(n) < sizeof(buf) ? static_cast<size_t>(n)
                                                    : sizeof(buf) - 1;
  Write(level, std::string_view(buf, len));
}

}  // namespace aeonian::log
