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

#include "host/runtime.h"

#include <algorithm>
#include <cstring>

#include "core/cmdline.h"
#include "core/ini.h"
#include "core/pe_image.h"
#include "host/hooks.h"
#include "host/inline_hook.h"
#include "host/video.h"
#include "host/win_util.h"

#ifndef AEONIAN_VERSION
#define AEONIAN_VERSION "dev"
#endif

namespace aeonian {

namespace {

constexpr size_t kMaxTrackedPaths = 20000;

LONG WINAPI CrashLogger(EXCEPTION_POINTERS* info) {
  static volatile LONG reported = 0;
  DWORD code = info->ExceptionRecord->ExceptionCode;
  bool interesting = code == EXCEPTION_ACCESS_VIOLATION ||
                     code == EXCEPTION_ILLEGAL_INSTRUCTION ||
                     code == EXCEPTION_STACK_OVERFLOW ||
                     code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
                     code == EXCEPTION_PRIV_INSTRUCTION;
  if (interesting && InterlockedIncrement(&reported) <= 8) {
    void* address = info->ExceptionRecord->ExceptionAddress;
    HMODULE module = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCSTR>(address), &module);
    std::string where = module != nullptr ? ModulePath(module) : "?";
    size_t cut = where.find_last_of('\\');
    if (cut != std::string::npos) where.erase(0, cut + 1);
    log::Writef(log::Level::kWarn,
                "excecao de 1a chance 0x%08lx em %p (%s+0x%lx)",
                static_cast<unsigned long>(code), address, where.c_str(),
                static_cast<unsigned long>(
                    reinterpret_cast<uintptr_t>(address) -
                    reinterpret_cast<uintptr_t>(module)));
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

// --- API publica -----------------------------------------------------------

const PluginInstance& Self(const AeonianHost* host) {
  return *reinterpret_cast<const PluginInstance*>(host);
}

void ApiLog(const AeonianHost* host, AeonianLogLevel level, const char* msg) {
  if (host == nullptr || msg == nullptr) return;
  Runtime::Get().PluginLog(Self(host), level, msg);
}

int ApiRegisterEvents(const AeonianHost* host, const AeonianEvents* events) {
  if (host == nullptr || events == nullptr ||
      events->struct_size < sizeof(uint32_t) + sizeof(void*)) {
    return AEONIAN_ERR_ARGUMENT;
  }
  return Runtime::Get().RegisterEvents(Self(host), *events);
}

uintptr_t ApiResolveSymbol(const AeonianHost* host, const char* name) {
  if (host == nullptr || name == nullptr) return 0;
  return Runtime::Get().ResolveSymbol(name);
}

int ApiHookInline(const AeonianHost* host, uintptr_t target,
                  const uint8_t* expected, uint32_t length, void* detour,
                  void** trampoline) {
  if (host == nullptr) return AEONIAN_ERR_ARGUMENT;
  int rc = InstallInlineHook(target, expected, length, detour, trampoline,
                             Self(host).mod_id.c_str());
  if (rc != AEONIAN_OK) {
    log::Writef(log::Level::kWarn, "[%s] hook_inline(0x%08lx) falhou: %d",
                Self(host).mod_id.c_str(), static_cast<unsigned long>(target),
                rc);
  }
  return rc;
}

int ApiPatchBytes(const AeonianHost* host, uintptr_t target,
                  const uint8_t* expected, const uint8_t* replacement,
                  uint32_t length) {
  if (host == nullptr) return AEONIAN_ERR_ARGUMENT;
  return PatchBytes(target, expected, replacement, length,
                    Self(host).mod_id.c_str());
}

int ApiReadOriginal(const AeonianHost* host, uintptr_t address, void* out,
                    uint32_t length) {
  if (host == nullptr) return AEONIAN_ERR_ARGUMENT;
  return ReadOriginal(address, static_cast<uint8_t*>(out), length, nullptr);
}

int ApiRequestScreenshot(const AeonianHost* host, const char* path) {
  if (host == nullptr) return AEONIAN_ERR_ARGUMENT;
  return Runtime::Get().RequestScreenshot(path);
}

uint32_t ApiModCount(const AeonianHost* host) {
  return host == nullptr ? 0 : Runtime::Get().ModCount();
}

int ApiModInfo(const AeonianHost* host, uint32_t index, AeonianModInfo* out) {
  if (host == nullptr || out == nullptr) return AEONIAN_ERR_ARGUMENT;
  return Runtime::Get().ModInfo(index, out) ? AEONIAN_OK : AEONIAN_ERR_ARGUMENT;
}

}  // namespace

Runtime& Runtime::Get() {
  static Runtime instance;
  return instance;
}

// --- Bootstrap (sob o loader lock) ------------------------------------------

void Runtime::Bootstrap(HMODULE self) {
  if (bootstrapped_) return;
  bootstrapped_ = true;
  start_ms_ = GetTickCount();
  InitializeCriticalSection(&init_lock_);
  InitializeCriticalSection(&trace_lock_);

  self_ = self;
  aeonian_dir_ = DirName(ModulePath(self));
  game_dir_ = DirName(ModulePath(nullptr));
  LoadConfig();
  log::Init(aeonian_dir_ + "\\aeonian.log", config_.log_level);
  AddVectoredExceptionHandler(1, CrashLogger);

  game_base_ = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
  log::Writef(log::Level::kInfo, "Aeonian %s iniciado", AEONIAN_VERSION);
  log::Writef(log::Level::kInfo, "jogo: %s (base 0x%08lx)", game_dir_.c_str(),
              static_cast<unsigned long>(game_base_));

  if (!IdentifyBuild()) return;  // build desconhecida: nenhum hook instalado
  InstallHooks(reinterpret_cast<uint8_t*>(game_base_), pe_.size_of_image);
}

void Runtime::LoadConfig() {
  std::string text;
  if (!ReadWholeFile(aeonian_dir_ + "\\aeonian.ini", &text)) return;
  Ini ini = Ini::Parse(text);
  config_.log_level =
      log::ParseLevel(ini.GetOr("core", "log_level", "info"), log::Level::kInfo);
  std::string direct = ToLowerAscii(ini.GetOr("core", "force_direct", "auto"));
  config_.force_direct = direct == "on"    ? Config::Direct::kOn
                         : direct == "off" ? Config::Direct::kOff
                                           : Config::Direct::kAuto;
  config_.allow_unknown_build = ini.GetBool("core", "allow_unknown_build", false);
  config_.trace_files = ini.GetBool("core", "trace_files", false);
  config_.extra_args = SplitList(ini.GetOr("core", "extra_args", ""));
  config_.skip_videos = ToLowerAscii(ini.GetOr("game", "skip_videos", ""));
  {
    const std::string v = ToLowerAscii(ini.GetOr("game", "skip_intro", "true"));
    config_.skip_intro_count = v == "false" || v == "off" || v == "no" ? 0
                               : static_cast<int>(std::clamp<int64_t>(
                                     ini.GetInt("game", "skip_intro", 2), 0, 16));
    if (v == "true" || v == "on" || v == "yes") config_.skip_intro_count = 2;
    config_.skip_intro_window_s = static_cast<int>(
        std::clamp<int64_t>(ini.GetInt("game", "skip_intro_window", 30), 1, 600));
  }
  config_.video_backend = ToLowerAscii(ini.GetOr("video", "backend", "auto"));
  config_.d2gl_mode = ToLowerAscii(ini.GetOr("video", "d2gl_mode", "auto"));
  config_.overlay_max_width = static_cast<int>(
      std::clamp<int64_t>(ini.GetInt("video", "overlay_max_width", 1920), 320, 7680));
  config_.overlay_hz = static_cast<int>(
      std::clamp<int64_t>(ini.GetInt("video", "overlay_hz", 60), 1, 240));
  for (const std::string& m : SplitList(ini.GetOr("mods", "disabled", ""))) {
    config_.disabled_mods.push_back(ToLowerAscii(m));
  }
}

bool Runtime::IdentifyBuild() {
  const uint8_t* base = reinterpret_cast<const uint8_t*>(game_base_);
  // Os cabecalhos cabem na primeira pagina da imagem.
  if (!ParsePeHeaders(base, 0x1000, &pe_)) {
    log::Write(log::Level::kError, "cabecalho PE do jogo ilegivel; desativado");
    return false;
  }
  log::Writef(log::Level::kInfo,
              "Game.exe: timestamp 0x%08x, imagem 0x%x, entry rva 0x%x",
              pe_.timestamp, pe_.size_of_image, pe_.entry_rva);

  const std::string dir = aeonian_dir_ + "\\addresses";
  for (const std::string& name : ListDir(dir, "*.ini", false)) {
    std::string text;
    std::string error;
    if (!ReadWholeFile(dir + "\\" + name, &text)) continue;
    std::optional<AddressDb> db = AddressDb::Parse(text, &error);
    if (!db) {
      log::Writef(log::Level::kWarn, "addresses\\%s ignorado: %s", name.c_str(),
                  error.c_str());
      continue;
    }
    if (db->Matches(pe_)) {
      build_id_ = db->build_id();
      log::Writef(log::Level::kInfo, "build reconhecida: %s (%zu simbolos)",
                  db->build_name().c_str(), db->symbols().size());
      db_ = std::move(db);
      return true;
    }
  }
  if (config_.allow_unknown_build) {
    log::Write(log::Level::kWarn,
               "build NAO reconhecida, mas allow_unknown_build=1: hooks de "
               "arquivo/evento ativos, simbolos indisponiveis");
    return true;
  }
  log::Write(log::Level::kError,
             "build NAO reconhecida: Aeonian desativado para proteger o jogo "
             "(defina allow_unknown_build=1 em aeonian.ini para forcar)");
  return false;
}

// --- Inicializacao tardia (fora do loader lock) ------------------------------

void Runtime::EnsureInitialized() {
  if (initialized_.load(std::memory_order_acquire)) return;
  EnterCriticalSection(&init_lock_);
  if (!initialized_.load() && !initializing_) {
    initializing_ = true;
    DiscoverMods();
    LoadPlugins();
    initialized_.store(true, std::memory_order_release);
    log::Writef(log::Level::kInfo,
                "inicializado: %zu mod(s), %zu plugin(s), vfs=%s",
                mods_.size(), plugins_.size(), vfs_ ? "ativo" : "inativo");
  }
  LeaveCriticalSection(&init_lock_);
}

void Runtime::DiscoverMods() {
  const std::string root = aeonian_dir_ + "\\mods";
  const std::string build_name = db_ ? db_->build_name() : std::string();
  std::vector<std::string> seen_ids;

  for (const std::string& dir_name : ListDir(root, "*", true)) {
    const std::string dir = root + "\\" + dir_name;
    std::string text;
    if (!ReadWholeFile(dir + "\\mod.ini", &text)) {
      log::Writef(log::Level::kDebug, "mods\\%s ignorado: sem mod.ini",
                  dir_name.c_str());
      continue;
    }
    std::string error;
    std::optional<ModManifest> mod =
        ParseModManifest(text, ToLowerAscii(dir_name), &error);
    if (!mod) {
      log::Writef(log::Level::kError, "mods\\%s ignorado: %s", dir_name.c_str(),
                  error.c_str());
      continue;
    }
    mod->dir = dir;
    if (std::find(config_.disabled_mods.begin(), config_.disabled_mods.end(),
                  mod->id) != config_.disabled_mods.end()) {
      log::Writef(log::Level::kInfo, "mod '%s' desativado em aeonian.ini",
                  mod->id.c_str());
      continue;
    }
    if (!build_name.empty() && !SupportsBuild(*mod, build_name)) {
      log::Writef(log::Level::kWarn, "mod '%s' ignorado: nao suporta a build %s",
                  mod->id.c_str(), build_name.c_str());
      continue;
    }
    if (std::find(seen_ids.begin(), seen_ids.end(), mod->id) != seen_ids.end()) {
      log::Writef(log::Level::kError, "mod '%s' duplicado em mods\\%s; ignorado",
                  mod->id.c_str(), dir_name.c_str());
      continue;
    }
    seen_ids.push_back(mod->id);
    mods_.push_back(std::move(*mod));
  }
  SortForLoad(&mods_);

  std::vector<ModRoot> roots;
  for (const ModManifest& m : mods_) {
    const size_t files = index_.AddTree(m.dir + "\\data");
    mod_data_files_.push_back(static_cast<uint32_t>(files));
    log::Writef(log::Level::kInfo, "mod '%s' v%s (prioridade %d): %zu arquivo(s) em data\\, %zu plugin(s)",
                m.id.c_str(), m.version.c_str(), m.priority, files,
                m.plugins.size());
    roots.push_back(ModRoot{m.id, m.dir});
  }
  if (!roots.empty()) {
    // Consulta so o indice em memoria: nenhuma chamada ao SO por abertura.
    vfs_ = std::make_unique<Vfs>(
        game_dir_, std::move(roots),
        [this](const std::string& path) { return index_.Contains(path); });
  }
}

void Runtime::LoadPlugins() {
  for (const ModManifest& mod : mods_) {
    for (const std::string& plugin : mod.plugins) LoadPlugin(mod, plugin);
  }
}

bool Runtime::LoadPlugin(const ModManifest& mod, const std::string& relative) {
  std::string path = mod.dir + "\\" + relative;
  for (char& c : path) {
    if (c == '/') c = '\\';
  }
  HMODULE module =
      LoadLibraryExA(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  if (module == nullptr) {
    log::Writef(log::Level::kError, "[%s] falha ao carregar %s (erro %lu)",
                mod.id.c_str(), relative.c_str(), GetLastError());
    return false;
  }
  AeonianPluginInitFn init =
      ProcAddress<AeonianPluginInitFn>(module, AEONIAN_PLUGIN_INIT_SYMBOL);
  if (init == nullptr) {
    log::Writef(log::Level::kError, "[%s] %s nao exporta " AEONIAN_PLUGIN_INIT_SYMBOL,
                mod.id.c_str(), relative.c_str());
    FreeLibrary(module);
    return false;
  }

  auto plugin = std::make_unique<PluginInstance>();
  plugin->mod_id = mod.id;
  plugin->mod_dir = mod.dir;
  plugin->module = module;
  AeonianHost& host = plugin->host;
  std::memset(&host, 0, sizeof(host));
  host.struct_size = sizeof(AeonianHost);
  host.api_version = AEONIAN_API_VERSION;
  host.game_build = build_id_;
  host.game_base = game_base_;
  host.game_dir = game_dir_.c_str();
  host.mod_id = plugin->mod_id.c_str();
  host.mod_dir = plugin->mod_dir.c_str();
  host.log = ApiLog;
  host.register_events = ApiRegisterEvents;
  host.resolve_symbol = ApiResolveSymbol;
  host.hook_inline = ApiHookInline;
  host.patch_bytes = ApiPatchBytes;
  host.request_screenshot = ApiRequestScreenshot;
  host.mod_count = ApiModCount;
  host.mod_info = ApiModInfo;
  host.read_original = ApiReadOriginal;

  registering_ = plugin.get();
  int rc = init(&host);
  registering_ = nullptr;
  if (rc != 0) {
    log::Writef(log::Level::kWarn, "[%s] %s recusou carregar (retorno %d)",
                mod.id.c_str(), relative.c_str(), rc);
    // Remove os callbacks que ele registrou antes de recusar.
    subscribers_.erase(
        std::remove_if(subscribers_.begin(), subscribers_.end(),
                       [&](const Subscriber& s) { return s.mod_id == mod.id; }),
        subscribers_.end());
    want_present_.store(false);
    for (const Subscriber& s : subscribers_) {
      if (s.events.on_present != nullptr) want_present_.store(true);
    }
    FreeLibrary(module);
    return false;
  }
  log::Writef(log::Level::kInfo, "[%s] plugin %s carregado", mod.id.c_str(),
              relative.c_str());
  plugins_.push_back(std::move(plugin));
  return true;
}

// --- Servicos para os hooks --------------------------------------------------

bool Runtime::ShouldForceDirect() const {
  switch (config_.force_direct) {
    case Config::Direct::kOn: return true;
    case Config::Direct::kOff: return false;
    case Config::Direct::kAuto: break;
  }
  for (const ModManifest& m : mods_) {
    if (DirExists(m.dir + "\\data")) return true;
  }
  return false;
}

// O D2GL e um glide3x.dll (OpenGL) que o jogo carrega com -3dfx. "auto" o usa
// quando ele esta instalado na pasta do jogo.
// A DLL e o wrapper do D2GL? (o D2GL grava o nome "d2gl.mpq" em ambos.)
static bool LooksLikeD2gl(const std::string& path) {
  std::string data;
  return ReadWholeFile(path, &data) && data.find("d2gl.mpq") != std::string::npos;
}

bool Runtime::DecideD2gl() {
  d2gl_mode_.clear();
  if (config_.video_backend == "software") return false;
  const bool assets = FileExists(game_dir_ + "\\d2gl.mpq") || FileExists(game_dir_ + "\\d2gl.ini");
  const bool has_ddraw = assets && FileExists(game_dir_ + "\\ddraw.dll") &&
                         LooksLikeD2gl(game_dir_ + "\\ddraw.dll");
  const bool has_glide = assets && FileExists(game_dir_ + "\\glide3x.dll") &&
                         LooksLikeD2gl(game_dir_ + "\\glide3x.dll");
  std::string mode = config_.d2gl_mode;
  if (mode == "ddraw" && !has_ddraw) mode.clear();
  else if (mode == "glide" && !has_glide) mode.clear();
  else if (mode != "ddraw" && mode != "glide") {
    // auto: prefere Glide (-3dfx), o caminho validado sob Wine e em consoles; o
    // wrapper DDraw so e usado se for o unico instalado.
    mode = has_glide ? "glide" : has_ddraw ? "ddraw" : "";
  }
  if (mode.empty()) {
    if (config_.video_backend == "d2gl") {
      log::Write(log::Level::kWarn,
                 "video.backend=d2gl mas o D2GL (ddraw.dll/glide3x.dll + d2gl.mpq) nao esta na "
                 "pasta do jogo; usando o renderizador original (ae_install.py --d2gl-dir)");
    }
    return false;
  }
  d2gl_mode_ = mode;
  return true;
}

// Pula videos cujo nome contem algum trecho de [game] skip_videos. "all" pula
// todos; nomes desconhecidos (video em memoria) nunca sao pulados.
bool Runtime::ShouldSkipVideo(const std::string& name) const {
  if (config_.skip_videos.empty() || name.empty()) return false;
  const std::string lower = ToLowerAscii(name);
  for (const std::string& token : SplitList(config_.skip_videos)) {
    if (token == "all" || lower.find(token) != std::string::npos) return true;
  }
  return false;
}

bool Runtime::ShouldSkipIntroVideo() {
  if (config_.skip_intro_count <= 0) return false;
  if (GetTickCount() - start_ms_ > static_cast<DWORD>(config_.skip_intro_window_s) * 1000u) {
    return false;
  }
  return intro_skipped_.fetch_add(1) < config_.skip_intro_count;
}

const char* Runtime::CommandLine(const char* original) {
  EnsureInitialized();
  if (command_line_.empty()) {
    std::vector<std::string> extra = config_.extra_args;
    std::vector<std::string> remove;
    using_d2gl_ = DecideD2gl();
    if (using_d2gl_ && d2gl_mode_ == "glide") {
      extra.emplace_back("-3dfx");
      remove.emplace_back("-w");  // o wrapper Glide do D2GL e incompativel com -w
    }
    log::Writef(log::Level::kInfo, "video: backend=%s (%s)", config_.video_backend.c_str(),
                using_d2gl_ ? (d2gl_mode_ == "ddraw" ? "D2GL/OpenGL via ddraw.dll"
                                                     : "D2GL/OpenGL via glide3x.dll (-3dfx)")
                            : "renderizador original GDI/DirectDraw");
    for (const ModManifest& m : mods_) {
      extra.insert(extra.end(), m.args.begin(), m.args.end());
    }
    if (ShouldForceDirect()) extra.emplace_back("-direct");
    command_line_ = BuildCommandLine(original != nullptr ? original : "", extra, remove);
    if (command_line_ != (original != nullptr ? original : "")) {
      log::Writef(log::Level::kInfo, "linha de comando: %s",
                  command_line_.c_str());
    }
  }
  return command_line_.c_str();
}

bool Runtime::RedirectPath(const char* path, std::string* redirected) {
  if (!vfs_ || path == nullptr) return false;
  std::optional<std::string> rel = vfs_->RelativeDataPath(path);
  if (!rel) return false;
  std::string owner;
  std::optional<std::string> mapped = vfs_->Resolve(path, &owner);

  bool first_time = false;
  if (mapped || config_.trace_files) {
    EnterCriticalSection(&trace_lock_);
    if (seen_paths_.size() < kMaxTrackedPaths) {
      first_time = seen_paths_.insert(ToLowerAscii(*rel)).second;
    }
    LeaveCriticalSection(&trace_lock_);
  }
  if (first_time) {
    if (mapped) {
      log::Writef(log::Level::kInfo, "vfs: data\\%s -> mod '%s'", rel->c_str(),
                  owner.c_str());
    } else {
      log::Writef(log::Level::kDebug, "trace: data\\%s (jogo)", rel->c_str());
    }
  }
  if (!mapped) return false;
  redirects_.fetch_add(1, std::memory_order_relaxed);
  *redirected = std::move(*mapped);
  return true;
}

void Runtime::OnMessagePump() {
  const DWORD tid = GetCurrentThreadId();
  DWORD expected = 0;
  if (main_thread_.compare_exchange_strong(expected, tid)) {
    log::Writef(log::Level::kInfo,
                "laco de mensagens do jogo alcancado (thread principal T%04lx)",
                static_cast<unsigned long>(tid));
  }
  if (main_thread_.load() != tid) return;
  const uint32_t tick = tick_.fetch_add(1) + 1;
  if (tick == 1) {
    for (const Subscriber& s : subscribers_) {
      if (s.events.on_loaded != nullptr) s.events.on_loaded(s.events.user);
    }
  }
  for (const Subscriber& s : subscribers_) {
    if (s.events.on_tick != nullptr) s.events.on_tick(s.events.user, tick);
  }
}

bool Runtime::OnWindowMessage(AeonianWindowMessage* message) {
  for (const Subscriber& s : subscribers_) {
    if (s.events.on_window_message != nullptr &&
        s.events.on_window_message(s.events.user, message) != 0) {
      return true;
    }
  }
  return false;
}

void Runtime::OnShutdown() {
  if (shutdown_sent_.exchange(true)) return;
  log::Writef(log::Level::kInfo,
              "encerrando apos %u tick(s); %u abertura(s) de arquivo redirecionada(s)",
              tick_.load(), redirects_.load());
  for (const Subscriber& s : subscribers_) {
    if (s.events.on_shutdown != nullptr) s.events.on_shutdown(s.events.user);
  }
}

// --- Servicos da API publica -------------------------------------------------

void Runtime::PluginLog(const PluginInstance& plugin, AeonianLogLevel level,
                        const char* message) const {
  log::Level lv = level <= AEONIAN_LOG_DEBUG  ? log::Level::kDebug
                  : level == AEONIAN_LOG_INFO ? log::Level::kInfo
                  : level == AEONIAN_LOG_WARN ? log::Level::kWarn
                                              : log::Level::kError;
  log::Writef(lv, "[%s] %s", plugin.mod_id.c_str(), message);
}

int Runtime::RegisterEvents(const PluginInstance& plugin,
                            const AeonianEvents& events) {
  if (registering_ != &plugin) return AEONIAN_ERR_STATE;
  Subscriber sub;
  sub.mod_id = plugin.mod_id;
  std::memset(&sub.events, 0, sizeof(sub.events));
  std::memcpy(&sub.events, &events,
              std::min<size_t>(events.struct_size, sizeof(AeonianEvents)));
  if (sub.events.on_present != nullptr) want_present_.store(true);
  subscribers_.push_back(std::move(sub));
  return AEONIAN_OK;
}

void Runtime::DispatchPresent(AeonianCanvas* canvas) {
  for (const Subscriber& s : subscribers_) {
    if (s.events.on_present != nullptr) s.events.on_present(s.events.user, canvas);
  }
}

int Runtime::RequestScreenshot(const char* path) {
  if (path == nullptr || path[0] == '\0') return AEONIAN_ERR_ARGUMENT;
  EnterCriticalSection(&trace_lock_);
  screenshot_path_ = path;
  LeaveCriticalSection(&trace_lock_);
  return AEONIAN_OK;
}

bool Runtime::HasScreenshotRequest() {
  EnterCriticalSection(&trace_lock_);
  const bool has = !screenshot_path_.empty();
  LeaveCriticalSection(&trace_lock_);
  return has;
}

std::string Runtime::TakeScreenshotRequest() {
  EnterCriticalSection(&trace_lock_);
  std::string path;
  path.swap(screenshot_path_);
  LeaveCriticalSection(&trace_lock_);
  return path;
}

bool Runtime::ModInfo(uint32_t index, AeonianModInfo* out) const {
  if (index >= mods_.size() || out->struct_size < sizeof(uint32_t)) return false;
  const ModManifest& m = mods_[index];
  AeonianModInfo info;
  std::memset(&info, 0, sizeof(info));
  info.struct_size = sizeof(info);
  info.id = m.id.c_str();
  info.name = m.name.c_str();
  info.version = m.version.c_str();
  info.author = m.author.c_str();
  info.priority = m.priority;
  info.data_files = mod_data_files_[index];
  info.plugins = static_cast<uint32_t>(m.plugins.size());
  std::memcpy(out, &info, std::min<size_t>(out->struct_size, sizeof(info)));
  return true;
}

uintptr_t Runtime::ResolveSymbol(const char* name) const {
  if (!db_) return 0;
  const Symbol* symbol = db_->Find(name);
  if (symbol == nullptr) return 0;
  const uintptr_t address = game_base_ + symbol->rva;
  if (symbol->expect.empty()) return address;
  // Confere contra os bytes ORIGINAIS: se um mod ja desviou a funcao
  // (hook_inline/patch_bytes), o motor sabe o que estava la. Fora das faixas
  // que o proprio motor alterou, vale a memoria atual.
  const uint32_t length = static_cast<uint32_t>(symbol->expect.size());
  std::vector<uint8_t> current(length);
  std::string owners;
  if (ReadOriginal(address, current.data(), length, &owners) != AEONIAN_OK ||
      current != symbol->expect) {
    log::Writef(log::Level::kWarn, "simbolo %s: bytes nao conferem em 0x%08lx",
                name, static_cast<unsigned long>(address));
    return 0;
  }
  if (!owners.empty()) {
    log::Writef(log::Level::kDebug,
                "simbolo %s conferido com os bytes originais (alterado por %s)",
                name, owners.c_str());
  }
  return address;
}

}  // namespace aeonian
