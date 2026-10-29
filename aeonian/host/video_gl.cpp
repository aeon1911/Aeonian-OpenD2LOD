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

// aeonian/host/video_gl.cpp
//
// Backend OpenGL do on_present, para o modo D2GL (alto desempenho em monitores
// modernos). O jogo carrega o glide3x.dll do D2GL (-3dfx); trocamos a
// importacao GDI32!SwapBuffers DENTRO desse modulo. Antes de cada swap
// compomos a tela dos plugins como uma textura sobre o framebuffer padrao,
// salvando e restaurando todo o estado OpenGL que tocamos (o D2GL usa um
// contexto 3.3-4.6 proprio e nao pode perceber a nossa passagem).

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "aeonian/api.h"
#include "core/pe_image.h"
#include "host/log.h"
#include "host/runtime.h"
#include "host/video.h"

namespace aeonian {

LoadLibraryAFn g_real_load_library_a = nullptr;

namespace {

using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLboolean = unsigned char;
using GLbitfield = unsigned int;
using GLchar = char;

constexpr GLenum GL_TEXTURE_2D = 0x0DE1;
constexpr GLenum GL_BLEND = 0x0BE2;
constexpr GLenum GL_DEPTH_TEST = 0x0B71;
constexpr GLenum GL_CULL_FACE = 0x0B44;
constexpr GLenum GL_SCISSOR_TEST = 0x0C11;
constexpr GLenum GL_STENCIL_TEST = 0x0B90;
constexpr GLenum GL_FRAMEBUFFER_SRGB = 0x8DB9;
constexpr GLenum GL_SRC_ALPHA = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA = 0x0303;
constexpr GLenum GL_ONE = 1;
constexpr GLenum GL_FUNC_ADD = 0x8006;
constexpr GLenum GL_RGBA8 = 0x8058;
constexpr GLenum GL_BGRA = 0x80E1;
constexpr GLenum GL_BGR = 0x80E0;
constexpr GLenum GL_UNSIGNED_BYTE = 0x1401;
constexpr GLenum GL_TRIANGLES = 0x0004;
constexpr GLenum GL_LINEAR = 0x2601;
constexpr GLenum GL_NEAREST = 0x2600;
constexpr GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T = 0x2803;
constexpr GLenum GL_CLAMP_TO_EDGE = 0x812F;
constexpr GLenum GL_VIEWPORT = 0x0BA2;
constexpr GLenum GL_CURRENT_PROGRAM = 0x8B8D;
constexpr GLenum GL_VERTEX_ARRAY_BINDING = 0x85B5;
constexpr GLenum GL_TEXTURE_BINDING_2D = 0x8069;
constexpr GLenum GL_ACTIVE_TEXTURE = 0x84E0;
constexpr GLenum GL_TEXTURE0 = 0x84C0;
constexpr GLenum GL_SAMPLER_BINDING = 0x8919;
constexpr GLenum GL_BLEND_SRC_RGB = 0x80C9;
constexpr GLenum GL_BLEND_DST_RGB = 0x80C8;
constexpr GLenum GL_BLEND_SRC_ALPHA = 0x80CB;
constexpr GLenum GL_BLEND_DST_ALPHA = 0x80CA;
constexpr GLenum GL_BLEND_EQUATION_RGB = 0x8009;
constexpr GLenum GL_BLEND_EQUATION_ALPHA = 0x883D;
constexpr GLenum GL_FRAMEBUFFER = 0x8D40;
constexpr GLenum GL_READ_FRAMEBUFFER = 0x8CA8;
constexpr GLenum GL_DRAW_FRAMEBUFFER = 0x8CA9;
constexpr GLenum GL_DRAW_FRAMEBUFFER_BINDING = 0x8CA6;
constexpr GLenum GL_READ_FRAMEBUFFER_BINDING = 0x8CAA;
constexpr GLenum GL_VERTEX_SHADER = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS = 0x8B81;
constexpr GLenum GL_LINK_STATUS = 0x8B82;
constexpr GLenum GL_UNPACK_ALIGNMENT = 0x0CF5;
constexpr GLenum GL_UNPACK_ROW_LENGTH = 0x0CF2;
constexpr GLenum GL_PACK_ALIGNMENT = 0x0D05;
constexpr GLenum GL_PACK_ROW_LENGTH = 0x0D02;
constexpr GLenum GL_READ_BUFFER = 0x0C02;
constexpr GLenum GL_BACK = 0x0405;
constexpr GLenum GL_COLOR_WRITEMASK = 0x0C23;
constexpr GLenum GL_TRUE = 1;

#define GLCALL WINAPI

struct GlApi {
  // OpenGL 1.1 (exportadas por opengl32.dll)
  void(GLCALL* GetIntegerv)(GLenum, GLint*) = nullptr;
  void(GLCALL* GetBooleanv)(GLenum, GLboolean*) = nullptr;
  GLboolean(GLCALL* IsEnabled)(GLenum) = nullptr;
  void(GLCALL* Enable)(GLenum) = nullptr;
  void(GLCALL* Disable)(GLenum) = nullptr;
  void(GLCALL* BindTexture)(GLenum, GLuint) = nullptr;
  void(GLCALL* GenTextures)(GLsizei, GLuint*) = nullptr;
  void(GLCALL* TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                           const void*) = nullptr;
  void(GLCALL* TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                              const void*) = nullptr;
  void(GLCALL* TexParameteri)(GLenum, GLenum, GLint) = nullptr;
  void(GLCALL* DrawArrays)(GLenum, GLint, GLsizei) = nullptr;
  void(GLCALL* Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
  void(GLCALL* PixelStorei)(GLenum, GLint) = nullptr;
  void(GLCALL* ReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*) = nullptr;
  void(GLCALL* ReadBuffer)(GLenum) = nullptr;
  void(GLCALL* ColorMask)(GLboolean, GLboolean, GLboolean, GLboolean) = nullptr;
  GLenum(GLCALL* GetError)() = nullptr;
  // OpenGL 2.0+ (via wglGetProcAddress)
  GLuint(GLCALL* CreateShader)(GLenum) = nullptr;
  void(GLCALL* ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
  void(GLCALL* CompileShader)(GLuint) = nullptr;
  void(GLCALL* GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
  void(GLCALL* GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
  GLuint(GLCALL* CreateProgram)() = nullptr;
  void(GLCALL* AttachShader)(GLuint, GLuint) = nullptr;
  void(GLCALL* LinkProgram)(GLuint) = nullptr;
  void(GLCALL* GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
  void(GLCALL* GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
  void(GLCALL* UseProgram)(GLuint) = nullptr;
  void(GLCALL* DeleteShader)(GLuint) = nullptr;
  void(GLCALL* GenVertexArrays)(GLsizei, GLuint*) = nullptr;
  void(GLCALL* BindVertexArray)(GLuint) = nullptr;
  void(GLCALL* ActiveTexture)(GLenum) = nullptr;
  void(GLCALL* BindFramebuffer)(GLenum, GLuint) = nullptr;
  void(GLCALL* BlendFuncSeparate)(GLenum, GLenum, GLenum, GLenum) = nullptr;
  void(GLCALL* BlendEquationSeparate)(GLenum, GLenum) = nullptr;
  void(GLCALL* BindSampler)(GLuint, GLuint) = nullptr;  // opcional (GL 3.3)
};

struct State {
  GLint program = 0, vao = 0, active_texture = 0, texture0 = 0, sampler0 = 0;
  GLint draw_fbo = 0, read_fbo = 0, viewport[4] = {0, 0, 0, 0};
  GLint blend_src_rgb = 0, blend_dst_rgb = 0, blend_src_a = 0, blend_dst_a = 0;
  GLint blend_eq_rgb = 0, blend_eq_a = 0;
  GLint unpack_alignment = 4, unpack_row_length = 0, pack_alignment = 4, pack_row_length = 0;
  GLint read_buffer = 0;
  GLboolean blend = 0, depth = 0, cull = 0, scissor = 0, stencil = 0, srgb = 0;
  GLboolean color_mask[4] = {1, 1, 1, 1};
};

struct Gl {
  GlApi api;
  bool loaded = false;
  bool failed = false;  // erro definitivo: para de tentar
  HGLRC context = nullptr;
  GLuint program = 0, vao = 0, texture = 0;
  int tex_w = 0, tex_h = 0;
  bool has_content = false;  // a textura atual tem algo a desenhar
  DWORD last_render_ms = 0;
  uint32_t logged = 0;
  LONG busy = 0;
};
Gl g;

using SwapBuffersFn = BOOL(WINAPI*)(HDC);
SwapBuffersFn g_real_swap_buffers = nullptr;
bool g_glide_hooked = false;

constexpr char kVertexShader[] =
    "#version 330 core\n"
    "out vec2 uv;\n"
    "void main() {\n"
    "  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
    "  uv = vec2(p.x, 1.0 - p.y);\n"
    "  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);\n"
    "}\n";
constexpr char kFragmentShader[] =
    "#version 330 core\n"
    "in vec2 uv;\n"
    "out vec4 color;\n"
    "uniform sampler2D tex;\n"
    "void main() { color = texture(tex, uv); }\n";

template <typename Fn>
bool Load(Fn* out, HMODULE gl, const char* name, bool core11, bool required = true) {
  using WglGetProcAddress = PROC(WINAPI*)(LPCSTR);
  void* address = nullptr;
  if (!core11) {
    WglGetProcAddress get = nullptr;
    FARPROC raw = GetProcAddress(gl, "wglGetProcAddress");
    std::memcpy(&get, &raw, sizeof(get));
    if (get != nullptr) {
      PROC p = get(name);
      const uintptr_t v = reinterpret_cast<uintptr_t>(p);
      if (v > 3 && v != static_cast<uintptr_t>(-1)) address = reinterpret_cast<void*>(p);
    }
  }
  if (address == nullptr) {
    FARPROC raw = GetProcAddress(gl, name);
    std::memcpy(&address, &raw, sizeof(address));
  }
  if (address == nullptr) {
    if (required) log::Writef(log::Level::kWarn, "OpenGL: funcao %s indisponivel", name);
    return !required;
  }
  std::memcpy(out, &address, sizeof(address));
  return true;
}

bool LoadApi(GlApi* a) {
  HMODULE gl = GetModuleHandleA("opengl32.dll");
  if (gl == nullptr) return false;
  bool ok = true;
#define L11(field, name) ok = Load(&a->field, gl, name, true) && ok
#define LEXT(field, name) ok = Load(&a->field, gl, name, false) && ok
  L11(GetIntegerv, "glGetIntegerv");     L11(GetBooleanv, "glGetBooleanv");
  L11(IsEnabled, "glIsEnabled");         L11(Enable, "glEnable");
  L11(Disable, "glDisable");             L11(BindTexture, "glBindTexture");
  L11(GenTextures, "glGenTextures");     L11(TexImage2D, "glTexImage2D");
  L11(TexSubImage2D, "glTexSubImage2D"); L11(TexParameteri, "glTexParameteri");
  L11(DrawArrays, "glDrawArrays");       L11(Viewport, "glViewport");
  L11(PixelStorei, "glPixelStorei");     L11(ReadPixels, "glReadPixels");
  L11(ReadBuffer, "glReadBuffer");       L11(ColorMask, "glColorMask");
  L11(GetError, "glGetError");
  LEXT(CreateShader, "glCreateShader");  LEXT(ShaderSource, "glShaderSource");
  LEXT(CompileShader, "glCompileShader"); LEXT(GetShaderiv, "glGetShaderiv");
  LEXT(GetShaderInfoLog, "glGetShaderInfoLog"); LEXT(CreateProgram, "glCreateProgram");
  LEXT(AttachShader, "glAttachShader");  LEXT(LinkProgram, "glLinkProgram");
  LEXT(GetProgramiv, "glGetProgramiv");  LEXT(GetProgramInfoLog, "glGetProgramInfoLog");
  LEXT(UseProgram, "glUseProgram");      LEXT(DeleteShader, "glDeleteShader");
  LEXT(GenVertexArrays, "glGenVertexArrays"); LEXT(BindVertexArray, "glBindVertexArray");
  LEXT(ActiveTexture, "glActiveTexture"); LEXT(BindFramebuffer, "glBindFramebuffer");
  LEXT(BlendFuncSeparate, "glBlendFuncSeparate");
  LEXT(BlendEquationSeparate, "glBlendEquationSeparate");
  ok = Load(&a->BindSampler, gl, "glBindSampler", false, /*required=*/false) && ok;
#undef L11
#undef LEXT
  return ok;
}

GLuint CompileShader(GLenum type, const char* source) {
  const GlApi& a = g.api;
  const GLuint shader = a.CreateShader(type);
  a.ShaderSource(shader, 1, &source, nullptr);
  a.CompileShader(shader);
  GLint ok = 0;
  a.GetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (ok == 0) {
    char info[512] = {};
    a.GetShaderInfoLog(shader, sizeof(info) - 1, nullptr, info);
    log::Writef(log::Level::kError, "OpenGL: shader do overlay nao compilou: %s", info);
    return 0;
  }
  return shader;
}

bool BuildProgram() {
  const GlApi& a = g.api;
  const GLuint vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
  const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
  if (vs == 0 || fs == 0) return false;
  g.program = a.CreateProgram();
  a.AttachShader(g.program, vs);
  a.AttachShader(g.program, fs);
  a.LinkProgram(g.program);
  a.DeleteShader(vs);
  a.DeleteShader(fs);
  GLint ok = 0;
  a.GetProgramiv(g.program, GL_LINK_STATUS, &ok);
  if (ok == 0) {
    char info[512] = {};
    a.GetProgramInfoLog(g.program, sizeof(info) - 1, nullptr, info);
    log::Writef(log::Level::kError, "OpenGL: link do overlay falhou: %s", info);
    return false;
  }
  return true;
}

// Prepara (ou refaz, se o contexto mudou) os recursos do overlay.
bool EnsureGl() {
  if (g.failed) return false;
  if (!g.loaded) {
    if (!LoadApi(&g.api)) {
      g.failed = true;
      log::Write(log::Level::kError, "OpenGL: overlay desativado (funcoes ausentes)");
      return false;
    }
    g.loaded = true;
  }
  HMODULE gl = GetModuleHandleA("opengl32.dll");
  using GetCurrentContextFn = HGLRC(WINAPI*)();
  GetCurrentContextFn current = nullptr;
  FARPROC raw = GetProcAddress(gl, "wglGetCurrentContext");
  std::memcpy(&current, &raw, sizeof(current));
  const HGLRC ctx = current != nullptr ? current() : nullptr;
  if (ctx == nullptr) return false;  // sem contexto nesta thread
  if (ctx != g.context) {              // primeiro uso ou contexto recriado
    g.program = g.vao = g.texture = 0;
    g.tex_w = g.tex_h = 0;
    g.has_content = false;
    g.context = ctx;
    if (!BuildProgram()) {
      g.failed = true;
      return false;
    }
    g.api.GenVertexArrays(1, &g.vao);
    log::Writef(log::Level::kInfo, "OpenGL: overlay pronto (programa %u, contexto %p)", g.program,
                static_cast<void*>(ctx));
  }
  return true;
}

State SaveState() {
  const GlApi& a = g.api;
  State s;
  a.GetIntegerv(GL_CURRENT_PROGRAM, &s.program);
  a.GetIntegerv(GL_VERTEX_ARRAY_BINDING, &s.vao);
  a.GetIntegerv(GL_ACTIVE_TEXTURE, &s.active_texture);
  a.ActiveTexture(GL_TEXTURE0);
  a.GetIntegerv(GL_TEXTURE_BINDING_2D, &s.texture0);
  if (a.BindSampler != nullptr) a.GetIntegerv(GL_SAMPLER_BINDING, &s.sampler0);
  a.GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s.draw_fbo);
  a.GetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &s.read_fbo);
  a.GetIntegerv(GL_VIEWPORT, s.viewport);
  a.GetIntegerv(GL_BLEND_SRC_RGB, &s.blend_src_rgb);
  a.GetIntegerv(GL_BLEND_DST_RGB, &s.blend_dst_rgb);
  a.GetIntegerv(GL_BLEND_SRC_ALPHA, &s.blend_src_a);
  a.GetIntegerv(GL_BLEND_DST_ALPHA, &s.blend_dst_a);
  a.GetIntegerv(GL_BLEND_EQUATION_RGB, &s.blend_eq_rgb);
  a.GetIntegerv(GL_BLEND_EQUATION_ALPHA, &s.blend_eq_a);
  a.GetIntegerv(GL_UNPACK_ALIGNMENT, &s.unpack_alignment);
  a.GetIntegerv(GL_UNPACK_ROW_LENGTH, &s.unpack_row_length);
  a.GetIntegerv(GL_PACK_ALIGNMENT, &s.pack_alignment);
  a.GetIntegerv(GL_PACK_ROW_LENGTH, &s.pack_row_length);
  a.GetIntegerv(GL_READ_BUFFER, &s.read_buffer);
  a.GetBooleanv(GL_COLOR_WRITEMASK, s.color_mask);
  s.blend = a.IsEnabled(GL_BLEND);
  s.depth = a.IsEnabled(GL_DEPTH_TEST);
  s.cull = a.IsEnabled(GL_CULL_FACE);
  s.scissor = a.IsEnabled(GL_SCISSOR_TEST);
  s.stencil = a.IsEnabled(GL_STENCIL_TEST);
  s.srgb = a.IsEnabled(GL_FRAMEBUFFER_SRGB);
  return s;
}

void SetEnabled(GLenum cap, GLboolean on) {
  if (on != 0) {
    g.api.Enable(cap);
  } else {
    g.api.Disable(cap);
  }
}

void RestoreState(const State& s) {
  const GlApi& a = g.api;
  SetEnabled(GL_BLEND, s.blend);
  SetEnabled(GL_DEPTH_TEST, s.depth);
  SetEnabled(GL_CULL_FACE, s.cull);
  SetEnabled(GL_SCISSOR_TEST, s.scissor);
  SetEnabled(GL_STENCIL_TEST, s.stencil);
  SetEnabled(GL_FRAMEBUFFER_SRGB, s.srgb);
  a.ColorMask(s.color_mask[0], s.color_mask[1], s.color_mask[2], s.color_mask[3]);
  a.BlendFuncSeparate(static_cast<GLenum>(s.blend_src_rgb), static_cast<GLenum>(s.blend_dst_rgb),
                      static_cast<GLenum>(s.blend_src_a), static_cast<GLenum>(s.blend_dst_a));
  a.BlendEquationSeparate(static_cast<GLenum>(s.blend_eq_rgb), static_cast<GLenum>(s.blend_eq_a));
  a.PixelStorei(GL_UNPACK_ALIGNMENT, s.unpack_alignment);
  a.PixelStorei(GL_UNPACK_ROW_LENGTH, s.unpack_row_length);
  a.PixelStorei(GL_PACK_ALIGNMENT, s.pack_alignment);
  a.PixelStorei(GL_PACK_ROW_LENGTH, s.pack_row_length);
  a.BindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(s.draw_fbo));
  a.BindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(s.read_fbo));
  a.ReadBuffer(static_cast<GLenum>(s.read_buffer));
  a.Viewport(s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3]);
  a.UseProgram(static_cast<GLuint>(s.program));
  a.BindVertexArray(static_cast<GLuint>(s.vao));
  if (a.BindSampler != nullptr) a.BindSampler(0, static_cast<GLuint>(s.sampler0));
  a.BindTexture(GL_TEXTURE_2D, static_cast<GLuint>(s.texture0));
  a.ActiveTexture(static_cast<GLenum>(s.active_texture));
  while (a.GetError() != 0) {  // nao deixe erros nossos para o D2GL
  }
}

// Converte o DIB dos plugins em BGRA com alfa (cor-chave -> transparente) e o
// envia como textura. Devolve se ha algo visivel.
bool UploadCanvas(int w, int h, const CanvasPixels& px) {
  const GlApi& a = g.api;
  const uint32_t key = AEONIAN_CANVAS_KEY & 0x00FFFFFFu;
  const uint32_t alpha = (px.opacity & 0xFFu) << 24;
  bool any = false;
  const size_t count = static_cast<size_t>(w) * static_cast<size_t>(h);
  for (size_t i = 0; i < count; ++i) {
    const uint32_t rgb = px.bits[i] & 0x00FFFFFFu;
    if (rgb == key) {
      px.bits[i] = 0;
    } else {
      px.bits[i] = alpha | rgb;
      any = true;
    }
  }
  if (g.texture == 0) {
    a.GenTextures(1, &g.texture);
    g.tex_w = g.tex_h = 0;
  }
  a.ActiveTexture(GL_TEXTURE0);
  a.BindTexture(GL_TEXTURE_2D, g.texture);
  a.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
  a.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  if (g.tex_w != w || g.tex_h != h) {
    a.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(GL_CLAMP_TO_EDGE));
    a.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(GL_CLAMP_TO_EDGE));
    a.TexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(GL_RGBA8), w, h, 0, GL_BGRA,
                 GL_UNSIGNED_BYTE, px.bits);
    g.tex_w = w;
    g.tex_h = h;
  } else {
    a.TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, px.bits);
  }
  return any;
}

void DrawOverlay(int client_w, int client_h, bool scaled) {
  const GlApi& a = g.api;
  a.BindFramebuffer(GL_FRAMEBUFFER, 0);
  a.Viewport(0, 0, client_w, client_h);
  a.Disable(GL_DEPTH_TEST);
  a.Disable(GL_CULL_FACE);
  a.Disable(GL_SCISSOR_TEST);
  a.Disable(GL_STENCIL_TEST);
  a.Disable(GL_FRAMEBUFFER_SRGB);
  a.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  a.Enable(GL_BLEND);
  a.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  a.BlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
  a.ActiveTexture(GL_TEXTURE0);
  a.BindTexture(GL_TEXTURE_2D, g.texture);
  if (a.BindSampler != nullptr) a.BindSampler(0, 0);
  const GLint filter = static_cast<GLint>(scaled ? GL_LINEAR : GL_NEAREST);
  a.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  a.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  a.UseProgram(g.program);
  a.BindVertexArray(g.vao);
  a.DrawArrays(GL_TRIANGLES, 0, 3);
}

// Le o framebuffer padrao (ja com o overlay) e grava um BMP de 24 bits. So os
// pixels da janela do jogo; nunca a area de trabalho.
bool CaptureFramebuffer(const std::string& path, int w, int h) {
  const GlApi& a = g.api;
  a.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  a.ReadBuffer(GL_BACK);
  a.PixelStorei(GL_PACK_ALIGNMENT, 4);
  a.PixelStorei(GL_PACK_ROW_LENGTH, 0);
  const size_t stride = (static_cast<size_t>(w) * 3 + 3) & ~static_cast<size_t>(3);
  std::vector<uint8_t> pixels(stride * static_cast<size_t>(h));
  a.ReadPixels(0, 0, w, h, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());  // de baixo para cima = BMP
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + static_cast<DWORD>(pixels.size());
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(bih);
  bih.biWidth = w;
  bih.biHeight = h;
  bih.biPlanes = 1;
  bih.biBitCount = 24;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = static_cast<DWORD>(pixels.size());
  HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool ok = WriteFile(file, &bfh, sizeof(bfh), &written, nullptr) &&
                  WriteFile(file, &bih, sizeof(bih), &written, nullptr) &&
                  WriteFile(file, pixels.data(), static_cast<DWORD>(pixels.size()), &written, nullptr);
  CloseHandle(file);
  return ok;
}

void OverlayFrame(HDC hdc) {
  Runtime& rt = Runtime::Get();
  const bool want_overlay = rt.WantsPresent();
  const bool shot_pending = rt.HasScreenshotRequest();
  if (!want_overlay && !shot_pending) return;
  HWND window = WindowFromDC(hdc);
  RECT rc;
  if (window == nullptr || !GetClientRect(window, &rc)) return;
  const int cw = static_cast<int>(rc.right - rc.left);
  const int ch = static_cast<int>(rc.bottom - rc.top);
  if (cw <= 0 || ch <= 0 || cw > 16384 || ch > 16384) return;
  if (!EnsureGl()) return;

  // O canvas dos plugins tem no maximo `overlay_max_width` de largura (ate
  // em 4K); a textura e esticada sobre toda a janela.
  const double scale = std::min(1.0, static_cast<double>(rt.OverlayMaxWidth()) / cw);
  const int w = std::max(1, static_cast<int>(cw * scale));
  const int h = std::max(1, static_cast<int>(ch * scale));
  const DWORD now = GetTickCount();
  const DWORD period = 1000u / static_cast<DWORD>(std::max(1, rt.OverlayHz()));
  const bool due = want_overlay && (g.texture == 0 || w != g.tex_w || h != g.tex_h ||
                                    now - g.last_render_ms >= period);

  const State saved = SaveState();
  if (due) {
    CanvasPixels px;
    if (RenderPluginCanvas(w, h, &px)) {
      g.has_content = UploadCanvas(w, h, px);
      g.last_render_ms = now;
    }
  }
  if (!want_overlay) g.has_content = false;
  if (g.has_content && g.texture != 0) DrawOverlay(cw, ch, w != cw || h != ch);
  if (shot_pending) {
    const std::string path = rt.TakeScreenshotRequest();
    if (!path.empty()) {
      const bool ok = CaptureFramebuffer(path, cw, ch);
      log::Writef(ok ? log::Level::kInfo : log::Level::kWarn,
                  "screenshot %s: %s (%dx%d, OpenGL)", ok ? "gravado" : "FALHOU", path.c_str(), cw,
                  ch);
    }
  }
  RestoreState(saved);
  if (g.logged < 2) {
    ++g.logged;
    log::Writef(log::Level::kInfo,
                "OpenGL: janela %dx%d, canvas dos plugins %dx%d, redesenho a %d Hz (thread T%04lx)",
                cw, ch, w, h, rt.OverlayHz(), static_cast<unsigned long>(GetCurrentThreadId()));
  }
}

BOOL WINAPI HookSwapBuffers(HDC hdc) {
  if (InterlockedCompareExchange(&g.busy, 1, 0) == 0) {
    OverlayFrame(hdc);
    InterlockedExchange(&g.busy, 0);
  }
  return g_real_swap_buffers(hdc);
}

bool IsGlideName(LPCSTR name) {
  if (name == nullptr) return false;
  const char* base = name;
  for (const char* p = name; *p != '\0'; ++p) {
    if (*p == '\\' || *p == '/') base = p + 1;
  }
  return lstrcmpiA(base, "glide3x.dll") == 0 || lstrcmpiA(base, "glide3x") == 0;
}

// Troca GDI32!SwapBuffers na tabela de importacao do modulo do D2GL.
void HookGlideModule(HMODULE module) {
  if (g_glide_hooked || module == nullptr) return;
  uint8_t* base = reinterpret_cast<uint8_t*>(module);
  PeInfo pe;
  if (!ParsePeHeaders(base, 0x1000, &pe)) return;
  uint32_t* slot = FindImportSlot(base, pe.size_of_image, "GDI32.dll", "SwapBuffers");
  if (slot == nullptr) {
    log::Write(log::Level::kWarn,
               "glide3x.dll carregado, mas nao importa GDI32!SwapBuffers: nao parece o D2GL; "
               "sem overlay neste modo");
    return;
  }
  DWORD old = 0;
  if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old)) return;
  const uint32_t current = *slot;
  std::memcpy(&g_real_swap_buffers, &current, sizeof(current));
  SwapBuffersFn hook = &HookSwapBuffers;
  uint32_t replacement = 0;
  std::memcpy(&replacement, &hook, sizeof(replacement));
  *slot = replacement;
  DWORD ignored = 0;
  VirtualProtect(slot, sizeof(*slot), old, &ignored);
  g_glide_hooked = true;
  log::Write(log::Level::kInfo, "D2GL detectado (glide3x.dll): SwapBuffers interceptado para o overlay");
}

}  // namespace

HMODULE WINAPI HookLoadLibraryA(LPCSTR name) {
  HMODULE module = g_real_load_library_a(name);
  if (module != nullptr && IsGlideName(name)) HookGlideModule(module);
  return module;
}

}  // namespace aeonian
