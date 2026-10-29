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

#include "host/hooks.h"

#include <windows.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "aeonian/api.h"
#include "core/ini.h"
#include "core/pe_image.h"
#include "host/log.h"
#include "host/runtime.h"
#include "host/media.h"
#include "host/video.h"

namespace aeonian {

namespace {

using GetCommandLineA_t = LPSTR(WINAPI*)();
using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD,
                                      LPSECURITY_ATTRIBUTES, DWORD, DWORD,
                                      HANDLE);
using GetFileAttributesA_t = DWORD(WINAPI*)(LPCSTR);
using ExitProcess_t = VOID(WINAPI*)(UINT);
using PeekMessageA_t = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
using RegisterClassA_t = ATOM(WINAPI*)(const WNDCLASSA*);
using CreateWindowExA_t = HWND(WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int,
                                        int, int, HWND, HMENU, HINSTANCE,
                                        LPVOID);

struct Originals {
  GetCommandLineA_t get_command_line = nullptr;
  CreateFileA_t create_file = nullptr;
  GetFileAttributesA_t get_file_attributes = nullptr;
  ExitProcess_t exit_process = nullptr;
  PeekMessageA_t peek_message = nullptr;
  RegisterClassA_t register_class = nullptr;
  CreateWindowExA_t create_window = nullptr;
};
Originals g_real;

// Classes de janela registradas pelo executavel e janelas ja interceptadas.
CRITICAL_SECTION g_window_lock;
std::vector<std::string> g_classes;                 // minusculas
std::vector<std::pair<HWND, WNDPROC>> g_subclassed;

// Aberturas de LEITURA de arquivos existentes sao as unicas redirecionadas:
// saves, logs e qualquer escrita nunca sao tocados.
bool IsReadOnlyOpen(DWORD access, DWORD disposition) {
  const DWORD write_bits = GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA |
                           FILE_APPEND_DATA | DELETE;
  return (access & write_bits) == 0 && disposition == OPEN_EXISTING;
}

// --- Hooks ---------------------------------------------------------------

LPSTR WINAPI HookGetCommandLineA() {
  Runtime& rt = Runtime::Get();
  LPSTR original = g_real.get_command_line();
  // O buffer devolvido vive tanto quanto o Runtime (singleton).
  return const_cast<LPSTR>(rt.CommandLine(original));
}

HANDLE WINAPI HookCreateFileA(LPCSTR name, DWORD access, DWORD share,
                              LPSECURITY_ATTRIBUTES security, DWORD disposition,
                              DWORD flags, HANDLE templ) {
  Runtime& rt = Runtime::Get();
  rt.EnsureInitialized();
  std::string redirected;
  if (name != nullptr && IsReadOnlyOpen(access, disposition) &&
      rt.RedirectPath(name, &redirected)) {
    name = redirected.c_str();
  }
  HANDLE handle = g_real.create_file(name, access, share, security,
                                     disposition, flags, templ);
  if (handle == INVALID_HANDLE_VALUE && !redirected.empty()) {
    // O arquivo do mod existe no indice mas nao abriu (permissao, removido...).
    log::Writef(log::Level::kWarn, "vfs: falha ao abrir %s (erro %lu)",
                redirected.c_str(), GetLastError());
  }
  return handle;
}

DWORD WINAPI HookGetFileAttributesA(LPCSTR name) {
  Runtime& rt = Runtime::Get();
  rt.EnsureInitialized();
  std::string redirected;
  if (name != nullptr && rt.RedirectPath(name, &redirected)) {
    name = redirected.c_str();
  }
  return g_real.get_file_attributes(name);
}

VOID WINAPI HookExitProcess(UINT code) {
  Runtime::Get().OnShutdown();
  g_real.exit_process(code);
}

BOOL WINAPI HookPeekMessageA(LPMSG msg, HWND hwnd, UINT min, UINT max,
                             UINT remove) {
  BOOL result = g_real.peek_message(msg, hwnd, min, max, remove);
  Runtime::Get().OnMessagePump();
  return result;
}

// --- Subclasse da janela principal --------------------------------------------

WNDPROC OriginalProcFor(HWND hwnd) {
  WNDPROC found = nullptr;
  EnterCriticalSection(&g_window_lock);
  for (const auto& entry : g_subclassed) {
    if (entry.first == hwnd) found = entry.second;
  }
  LeaveCriticalSection(&g_window_lock);
  return found;
}

LRESULT CALLBACK SubclassProc(HWND hwnd, UINT message, WPARAM wparam,
                              LPARAM lparam) {
  WNDPROC original = OriginalProcFor(hwnd);
  if (original == nullptr) return DefWindowProcA(hwnd, message, wparam, lparam);

  AeonianWindowMessage msg;
  msg.hwnd = hwnd;
  msg.message = message;
  msg.wparam = static_cast<uintptr_t>(wparam);
  msg.lparam = static_cast<intptr_t>(lparam);
  msg.result = 0;
  if (Runtime::Get().OnWindowMessage(&msg)) {
    return static_cast<LRESULT>(msg.result);
  }
  LRESULT result = CallWindowProcA(original, hwnd, message, wparam, lparam);
  if (message == WM_NCDESTROY) {
    EnterCriticalSection(&g_window_lock);
    for (size_t i = 0; i < g_subclassed.size(); ++i) {
      if (g_subclassed[i].first == hwnd) {
        g_subclassed.erase(g_subclassed.begin() + static_cast<ptrdiff_t>(i));
        break;
      }
    }
    LeaveCriticalSection(&g_window_lock);
  }
  return result;
}

bool IsStringAtom(LPCSTR name) {
  return reinterpret_cast<uintptr_t>(name) > 0xFFFF;
}

ATOM WINAPI HookRegisterClassA(const WNDCLASSA* wc) {
  if (wc != nullptr && wc->lpszClassName != nullptr &&
      IsStringAtom(wc->lpszClassName)) {
    EnterCriticalSection(&g_window_lock);
    g_classes.push_back(ToLowerAscii(wc->lpszClassName));
    LeaveCriticalSection(&g_window_lock);
    log::Writef(log::Level::kDebug, "classe de janela registrada: '%s'",
                wc->lpszClassName);
  }
  return g_real.register_class(wc);
}

HWND WINAPI HookCreateWindowExA(DWORD ex_style, LPCSTR class_name,
                                LPCSTR title, DWORD style, int x, int y, int w,
                                int h, HWND parent, HMENU menu,
                                HINSTANCE instance, LPVOID param) {
  HWND hwnd = g_real.create_window(ex_style, class_name, title, style, x, y, w,
                                   h, parent, menu, instance, param);
  if (hwnd == nullptr || class_name == nullptr || !IsStringAtom(class_name) ||
      (style & WS_CHILD) != 0) {
    return hwnd;
  }
  const std::string lower = ToLowerAscii(class_name);
  bool ours = false;
  EnterCriticalSection(&g_window_lock);
  for (const std::string& c : g_classes) ours = ours || c == lower;
  LeaveCriticalSection(&g_window_lock);
  if (!ours) return hwnd;

  WNDPROC previous = reinterpret_cast<WNDPROC>(
      SetWindowLongA(hwnd, GWL_WNDPROC, reinterpret_cast<LONG>(&SubclassProc)));
  if (previous == nullptr) {
    log::Writef(log::Level::kWarn, "subclasse da janela '%s' falhou (erro %lu)",
                class_name, GetLastError());
    return hwnd;
  }
  EnterCriticalSection(&g_window_lock);
  g_subclassed.emplace_back(hwnd, previous);
  LeaveCriticalSection(&g_window_lock);
  log::Writef(log::Level::kInfo,
              "janela '%s' (\"%s\") interceptada para eventos de entrada",
              class_name, title != nullptr ? title : "");
  return hwnd;
}

// --- Instalacao ------------------------------------------------------------------

template <typename Fn>
bool HookImport(uint8_t* base, size_t size, const char* dll, const char* name,
                Fn hook, Fn* original) {
  uint32_t* slot = FindImportSlot(base, size, dll, name);
  if (slot == nullptr) {
    log::Writef(log::Level::kWarn, "importacao %s!%s ausente; nao interceptada",
                dll, name);
    return false;
  }
  uint32_t current = *slot;
  std::memcpy(original, &current, sizeof(current));
  DWORD old = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old)) {
    log::Writef(log::Level::kError, "VirtualProtect falhou em %s!%s", dll, name);
    return false;
  }
  uint32_t replacement = 0;
  std::memcpy(&replacement, &hook, sizeof(replacement));
  *slot = replacement;
  DWORD ignored = 0;
  VirtualProtect(slot, sizeof(*slot), old, &ignored);
  log::Writef(log::Level::kDebug, "IAT %s!%s interceptada", dll, name);
  return true;
}

}  // namespace

void InstallHooks(uint8_t* base, size_t size) {
  InitializeCriticalSection(&g_window_lock);
  int installed = 0;
  auto count = [&](bool ok) { installed += ok ? 1 : 0; };
  count(HookImport(base, size, "KERNEL32.dll", "GetCommandLineA",
                   &HookGetCommandLineA, &g_real.get_command_line));
  count(HookImport(base, size, "KERNEL32.dll", "CreateFileA", &HookCreateFileA,
                   &g_real.create_file));
  count(HookImport(base, size, "KERNEL32.dll", "GetFileAttributesA",
                   &HookGetFileAttributesA, &g_real.get_file_attributes));
  count(HookImport(base, size, "KERNEL32.dll", "ExitProcess", &HookExitProcess,
                   &g_real.exit_process));
  count(HookImport(base, size, "USER32.dll", "PeekMessageA", &HookPeekMessageA,
                   &g_real.peek_message));
  count(HookImport(base, size, "USER32.dll", "RegisterClassA",
                   &HookRegisterClassA, &g_real.register_class));
  count(HookImport(base, size, "USER32.dll", "CreateWindowExA",
                   &HookCreateWindowExA, &g_real.create_window));
  count(HookImport(base, size, "DDRAW.dll", "DirectDrawCreate",
                   &HookDirectDrawCreate, &g_real_direct_draw_create));
  InitVideo();
  count(HookImport(base, size, "binkw32.dll", "_BinkOpen@8", &HookBinkOpen, &g_real_bink_open));
  count(HookImport(base, size, "smackw32.dll", "_SmackOpen@12", &HookSmackOpen,
                   &g_real_smack_open));
  count(HookImport(base, size, "KERNEL32.dll", "LoadLibraryA", &HookLoadLibraryA,
                   &g_real_load_library_a));
  count(HookImport(base, size, "GDI32.dll", "CreateDIBSection", &HookCreateDIBSection,
                   &g_real_create_dib_section));
  count(HookImport(base, size, "GDI32.dll", "StretchBlt", &HookStretchBlt,
                   &g_real_stretch_blt));
  log::Writef(log::Level::kInfo, "%d/13 importacoes interceptadas", installed);
}

}  // namespace aeonian
