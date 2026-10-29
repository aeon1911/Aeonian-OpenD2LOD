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

// Plugin de exemplo: o menor mod com codigo possivel. Mostra o ciclo de vida
// (init -> on_loaded -> on_tick -> on_shutdown) e como ouvir a janela do jogo.
//
// Para usar: copie hello.dll e mod.ini para <jogo>/aeonian/mods/hello/ e
// veja <jogo>/aeonian/aeonian.log.

#include <windows.h>

#include <cstdio>

#include "aeonian/api.h"

namespace {

const AeonianHost* g_host = nullptr;

void Logf(AeonianLogLevel level, const char* format, unsigned value) {
  char buf[128];
  std::snprintf(buf, sizeof(buf), format, value);
  g_host->log(g_host, level, buf);
}

void OnLoaded(void*) {
  g_host->log(g_host, AEONIAN_LOG_INFO, "o jogo chegou ao laco de mensagens");
}

void OnTick(void*, uint32_t tick) {
  if (tick % 1000 == 0) Logf(AEONIAN_LOG_DEBUG, "tick %u", tick);
}

int OnWindowMessage(void*, AeonianWindowMessage* msg) {
  // Exemplo: observa F10 sem consumi-la (retornar 0 deixa o jogo processar).
  if (msg->message == WM_KEYDOWN && msg->wparam == VK_F10) {
    g_host->log(g_host, AEONIAN_LOG_INFO, "F10 pressionada");
  }
  return 0;
}

void OnShutdown(void*) {
  g_host->log(g_host, AEONIAN_LOG_INFO, "ate a proxima");
}

}  // namespace

extern "C" AEONIAN_PLUGIN_EXPORT int AeonianPluginInit(const AeonianHost* host) {
  g_host = host;
  AeonianEvents events = {};
  events.struct_size = sizeof(events);
  events.on_loaded = OnLoaded;
  events.on_tick = OnTick;
  events.on_window_message = OnWindowMessage;
  events.on_shutdown = OnShutdown;
  return host->register_events(host, &events);
}
