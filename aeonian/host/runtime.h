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

// aeonian/host/runtime.h
//
// Estado e orquestracao do Aeonian dentro do processo do Game.exe.
//
// Ciclo de vida:
//   Bootstrap()          sob o loader lock (via proxy ijl11.dll): so le a
//                        configuracao, identifica a build e instala os hooks
//                        de IAT. Nada pesado.
//   EnsureInitialized()  na primeira chamada interceptada (ja fora do loader
//                        lock): descobre mods e carrega plugins.

#ifndef AEONIAN_HOST_RUNTIME_H_
#define AEONIAN_HOST_RUNTIME_H_

#include <windows.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aeonian/api.h"
#include "core/address_db.h"
#include "core/mod_manifest.h"
#include "core/vfs.h"
#include "host/file_index.h"
#include "host/log.h"

namespace aeonian {

struct Config {
  enum class Direct { kAuto, kOn, kOff };
  log::Level log_level = log::Level::kInfo;
  Direct force_direct = Direct::kAuto;
  bool allow_unknown_build = false;
  bool trace_files = false;
  std::vector<std::string> extra_args;
  std::vector<std::string> disabled_mods;
  // [video]
  std::string video_backend = "auto";  // auto | d2gl | software
  std::string d2gl_mode = "auto";      // auto | ddraw | glide
  int overlay_max_width = 1920;        // largura maxima do canvas (modo D2GL)
  int overlay_hz = 60;                 // taxa de redesenho do canvas (modo D2GL)
  // [game]
  std::string skip_videos = "";        // lista de trechos de nome de video a pular, ou "intro"
  int skip_intro_count = 2;            // quantos videos de abertura pular (0 = nenhum)
  int skip_intro_window_s = 30;        // so nos primeiros N segundos do processo
};

struct PluginInstance {
  AeonianHost host;  // DEVE ser o primeiro membro (vira `const AeonianHost*`)
  std::string mod_id;
  std::string mod_dir;
  HMODULE module = nullptr;
};

class Runtime {
 public:
  static Runtime& Get();

  void Bootstrap(HMODULE self);
  void EnsureInitialized();

  // Servicos usados pelos hooks.
  const char* CommandLine(const char* original);
  bool RedirectPath(const char* path, std::string* redirected);
  void OnMessagePump();
  bool OnWindowMessage(AeonianWindowMessage* message);
  void OnShutdown();

  // Apresentacao de quadro (host/video.cpp).
  bool WantsPresent() const { return want_present_; }
  int OverlayMaxWidth() const { return config_.overlay_max_width; }
  int OverlayHz() const { return config_.overlay_hz; }
  bool UsingD2gl() const { return using_d2gl_; }
  const std::string& D2glMode() const { return d2gl_mode_; }
  // O jogo vai abrir o video `name` (vazio = desconhecido). Verdadeiro => pular.
  bool ShouldSkipVideo(const std::string& name) const;
  // Video aberto por handle/memoria (sem nome): pula os primeiros do processo.
  bool ShouldSkipIntroVideo();
  void DispatchPresent(AeonianCanvas* canvas);
  std::string TakeScreenshotRequest();
  bool HasScreenshotRequest();

  // Servicos expostos pela API publica (api.h).
  void PluginLog(const PluginInstance& plugin, AeonianLogLevel level,
                 const char* message) const;
  int RegisterEvents(const PluginInstance& plugin, const AeonianEvents& events);
  uintptr_t ResolveSymbol(const char* name) const;
  int RequestScreenshot(const char* path);
  uint32_t ModCount() const { return static_cast<uint32_t>(mods_.size()); }
  bool ModInfo(uint32_t index, AeonianModInfo* out) const;

  uintptr_t game_base() const { return game_base_; }

 private:
  Runtime() = default;

  void LoadConfig();
  bool IdentifyBuild();
  void DiscoverMods();
  void LoadPlugins();
  bool LoadPlugin(const ModManifest& mod, const std::string& relative);
  bool ShouldForceDirect() const;
  bool DecideD2gl();

  struct Subscriber {
    std::string mod_id;
    AeonianEvents events;
  };

  HMODULE self_ = nullptr;
  std::string game_dir_;
  std::string aeonian_dir_;
  uintptr_t game_base_ = 0;
  PeInfo pe_;
  std::optional<AddressDb> db_;
  uint32_t build_id_ = AEONIAN_BUILD_UNKNOWN;
  Config config_;

  std::vector<ModManifest> mods_;
  std::vector<uint32_t> mod_data_files_;  // paralelo a mods_
  FileIndex index_;
  std::unique_ptr<Vfs> vfs_;
  std::vector<std::unique_ptr<PluginInstance>> plugins_;
  std::vector<Subscriber> subscribers_;
  PluginInstance* registering_ = nullptr;  // plugin dentro de PluginInit

  CRITICAL_SECTION init_lock_;
  CRITICAL_SECTION trace_lock_;
  std::atomic<bool> initialized_{false};
  bool initializing_ = false;
  bool bootstrapped_ = false;
  std::string command_line_;
  bool using_d2gl_ = false;
  std::string d2gl_mode_;  // "ddraw" | "glide" quando using_d2gl_
  std::unordered_set<std::string> seen_paths_;

  std::atomic<DWORD> main_thread_{0};
  std::atomic<uint32_t> tick_{0};
  std::atomic<uint32_t> redirects_{0};
  std::atomic<int> intro_skipped_{0};
  DWORD start_ms_ = 0;
  std::atomic<bool> want_present_{false};
  std::string screenshot_path_;  // protegido por trace_lock_
  std::atomic<bool> shutdown_sent_{false};
};

}  // namespace aeonian

#endif  // AEONIAN_HOST_RUNTIME_H_
