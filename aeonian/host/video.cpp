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

#include "host/video.h"

#include <windows.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-anonymous-struct"
#pragma clang diagnostic ignored "-Wnested-anon-types"
#pragma clang diagnostic ignored "-Wpedantic"
#endif
#include <ddraw.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "aeonian/api.h"
#include "host/log.h"
#include "host/runtime.h"

namespace aeonian {

DirectDrawCreateFn g_real_direct_draw_create = nullptr;
StretchBltFn g_real_stretch_blt = nullptr;
CreateDIBSectionFn g_real_create_dib_section = nullptr;

namespace {

// Posicoes nas tabelas de metodos COM (ordem das declaracoes em ddraw.h; as
// mesmas em IDirectDrawSurface 1..7 para os metodos usados aqui).
constexpr int kDdCreateSurface = 6;
constexpr int kSurfRelease = 2;
constexpr int kSurfBlt = 5;
constexpr int kSurfFlip = 11;
constexpr int kSurfGetAttachedSurface = 12;
constexpr int kSurfGetPalette = 20;
constexpr int kSurfGetSurfaceDesc = 22;
constexpr int kSurfLock = 25;
constexpr int kSurfUnlock = 32;
constexpr int kPalRelease = 2;
constexpr int kPalGetEntries = 4;

using CreateSurfaceFn = HRESULT(WINAPI*)(void*, DDSURFACEDESC*, void**, IUnknown*);
using BltFn = HRESULT(WINAPI*)(void*, RECT*, void*, RECT*, DWORD, void*);
using FlipFn = HRESULT(WINAPI*)(void*, void*, DWORD);
using GetAttachedFn = HRESULT(WINAPI*)(void*, DDSCAPS*, void**);
using GetPaletteFn = HRESULT(WINAPI*)(void*, void**);
using GetDescFn = HRESULT(WINAPI*)(void*, DDSURFACEDESC*);
using LockFn = HRESULT(WINAPI*)(void*, RECT*, DDSURFACEDESC*, DWORD, HANDLE);
using UnlockFn = HRESULT(WINAPI*)(void*, void*);
using ReleaseFn = ULONG(WINAPI*)(void*);
using PalGetEntriesFn = HRESULT(WINAPI*)(void*, DWORD, DWORD, DWORD, PALETTEENTRY*);

void** VtableOf(void* object) { return *reinterpret_cast<void***>(object); }

template <typename Fn>
Fn Slot(void** vtable, int index) {
  Fn fn = nullptr;
  std::memcpy(&fn, &vtable[index], sizeof(fn));
  return fn;
}

// Troca uma entrada da tabela de metodos (so se ainda nao for a nossa).
template <typename Fn>
bool PatchSlot(void** vtable, int index, Fn hook, Fn* original) {
  void* current = vtable[index];
  void* ours = nullptr;
  std::memcpy(&ours, &hook, sizeof(ours));
  if (current == ours) return true;
  DWORD old = 0;
  if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE, &old)) {
    return false;
  }
  std::memcpy(original, &current, sizeof(current));
  vtable[index] = ours;
  DWORD ignored = 0;
  VirtualProtect(&vtable[index], sizeof(void*), old, &ignored);
  return true;
}

// Formato de pixel de uma superficie bloqueada.
struct SurfaceFormat {
  int bits = 0;
  uint32_t rmask = 0, gmask = 0, bmask = 0;
  int rshift = 0, gshift = 0, bshift = 0;
  int rbits = 0, gbits = 0, bbits = 0;
};

void MaskInfo(uint32_t mask, int* shift, int* bits) {
  *shift = 0;
  *bits = 0;
  if (mask == 0) return;
  while ((mask & 1u) == 0) {
    mask >>= 1;
    ++*shift;
  }
  while ((mask & 1u) != 0) {
    mask >>= 1;
    ++*bits;
  }
}

struct Rgb {
  uint8_t r, g, b;
};

struct Video {
  CreateSurfaceFn create_surface = nullptr;
  BltFn blt = nullptr;
  FlipFn flip = nullptr;
  GetAttachedFn get_attached = nullptr;
  GetPaletteFn get_palette = nullptr;
  GetDescFn get_desc = nullptr;
  LockFn lock = nullptr;
  UnlockFn unlock = nullptr;

  void* primary = nullptr;
  bool dd_patched = false;
  bool surface_patched = false;
  LONG busy = 0;
  uint32_t frame = 0;
  uint32_t logged_blts = 0;
  uint32_t logged_gdi = 0;

  // Imagem GDI onde os plugins desenham.
  HDC dc = nullptr;
  HBITMAP bitmap = nullptr;
  HGDIOBJ old_bitmap = nullptr;
  uint32_t* dib = nullptr;
  int dib_w = 0, dib_h = 0;

  struct DibRecord {
    const void* bits;
    bool top_down;
    int width, height, bpp;
  };
  CRITICAL_SECTION dib_lock;
  bool dib_lock_ready = false;
  std::vector<DibRecord> dibs;  // DIBs criados pelo jogo (ultimos 64)

  PALETTEENTRY palette[256] = {};
  std::vector<int16_t> nearest;  // cache RGB555 -> indice, -1 = nao calculado
  std::vector<uint8_t> backup;   // pixels originais sob o overlay
};
Video g;

// --- Imagem GDI ---------------------------------------------------------------

bool EnsureDib(int w, int h) {
  if (g.dib != nullptr && g.dib_w == w && g.dib_h == h) return true;
  if (g.dc == nullptr) {
    g.dc = CreateCompatibleDC(nullptr);
    if (g.dc == nullptr) return false;
  }
  if (g.bitmap != nullptr) {
    SelectObject(g.dc, g.old_bitmap);
    DeleteObject(g.bitmap);
    g.bitmap = nullptr;
    g.dib = nullptr;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;  // de cima para baixo
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  g.bitmap = CreateDIBSection(g.dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (g.bitmap == nullptr || bits == nullptr) return false;
  g.old_bitmap = SelectObject(g.dc, g.bitmap);
  g.dib = static_cast<uint32_t*>(bits);
  g.dib_w = w;
  g.dib_h = h;
  return true;
}

// --- Conversao de cores --------------------------------------------------------

Rgb Unpack(const SurfaceFormat& f, const uint8_t* p) {
  switch (f.bits) {
    case 8: {
      const PALETTEENTRY& e = g.palette[*p];
      return Rgb{e.peRed, e.peGreen, e.peBlue};
    }
    case 16: {
      uint16_t v;
      std::memcpy(&v, p, 2);
      auto scale = [](uint32_t x, int bits) -> uint8_t {
        return bits == 0 ? 0 : static_cast<uint8_t>(x * 255u / ((1u << bits) - 1u));
      };
      return Rgb{scale((v & f.rmask) >> f.rshift, f.rbits),
                 scale((v & f.gmask) >> f.gshift, f.gbits),
                 scale((v & f.bmask) >> f.bshift, f.bbits)};
    }
    case 24:
      return Rgb{p[2], p[1], p[0]};
    default: {
      uint32_t v;
      std::memcpy(&v, p, 4);
      return Rgb{static_cast<uint8_t>((v & f.rmask) >> f.rshift),
                 static_cast<uint8_t>((v & f.gmask) >> f.gshift),
                 static_cast<uint8_t>((v & f.bmask) >> f.bshift)};
    }
  }
}

uint8_t NearestIndex(Rgb c) {
  const size_t key = (static_cast<size_t>(c.r >> 3) << 10) |
                     (static_cast<size_t>(c.g >> 3) << 5) | (c.b >> 3);
  if (g.nearest.empty()) g.nearest.assign(32768, -1);
  int16_t& cached = g.nearest[key];
  if (cached >= 0) return static_cast<uint8_t>(cached);
  int best = 0;
  int best_dist = 1 << 30;
  for (int i = 0; i < 256; ++i) {
    const int dr = static_cast<int>(g.palette[i].peRed) - c.r;
    const int dg = static_cast<int>(g.palette[i].peGreen) - c.g;
    const int db = static_cast<int>(g.palette[i].peBlue) - c.b;
    const int dist = dr * dr + dg * dg + db * db;
    if (dist < best_dist) {
      best_dist = dist;
      best = i;
    }
  }
  cached = static_cast<int16_t>(best);
  return static_cast<uint8_t>(best);
}

void Pack(const SurfaceFormat& f, uint8_t* p, Rgb c) {
  switch (f.bits) {
    case 8:
      *p = NearestIndex(c);
      break;
    case 16: {
      auto put = [](uint8_t v, int bits, int shift) -> uint32_t {
        return bits == 0 ? 0u : (static_cast<uint32_t>(v) >> (8 - bits)) << shift;
      };
      const uint16_t v = static_cast<uint16_t>(put(c.r, f.rbits, f.rshift) |
                                               put(c.g, f.gbits, f.gshift) |
                                               put(c.b, f.bbits, f.bshift));
      std::memcpy(p, &v, 2);
      break;
    }
    case 24:
      p[0] = c.b;
      p[1] = c.g;
      p[2] = c.r;
      break;
    default: {
      uint32_t v;
      std::memcpy(&v, p, 4);  // preserva o canal alfa/reservado
      v &= ~(f.rmask | f.gmask | f.bmask);
      v |= (static_cast<uint32_t>(c.r) << f.rshift) & f.rmask;
      v |= (static_cast<uint32_t>(c.g) << f.gshift) & f.gmask;
      v |= (static_cast<uint32_t>(c.b) << f.bshift) & f.bmask;
      std::memcpy(p, &v, 4);
      break;
    }
  }
}

// Le a paleta atual do jogo (superficie primaria) para o modo de 8 bits.
void RefreshPaletteDdraw() {
  if (g.primary == nullptr || g.get_palette == nullptr) return;
  void* pal = nullptr;
  if (FAILED(g.get_palette(g.primary, &pal)) || pal == nullptr) return;
  void** vt = VtableOf(pal);
  PALETTEENTRY now[256];
  if (SUCCEEDED(Slot<PalGetEntriesFn>(vt, kPalGetEntries)(pal, 0, 0, 256, now))) {
    if (std::memcmp(now, g.palette, sizeof(now)) != 0) {
      std::memcpy(g.palette, now, sizeof(now));
      g.nearest.assign(32768, -1);  // a paleta mudou: invalida o cache
    }
  }
  Slot<ReleaseFn>(vt, kPalRelease)(pal);
}

// --- Composicao (comum aos dois backends) ----------------------------------------

// Buffer de pixels acessivel pela CPU. A linha `y` (de cima para baixo) comeca
// em base + y * pitch; `pitch` pode ser negativo (DIB de baixo para cima).
struct PixelBuffer {
  uint8_t* base = nullptr;
  int pitch = 0;
  int width = 0;
  int height = 0;
  SurfaceFormat fmt;
};

uint8_t* RowPtr(const PixelBuffer& pb, int y) {
  return pb.base + static_cast<ptrdiff_t>(y) * pb.pitch;
}

struct Frame {
  bool has_overlay = false;
  int w = 0, h = 0;
  int bx0 = 0, by0 = 0, bx1 = -1, by1 = -1;  // caixa do que foi desenhado
  uint32_t opacity = 255;
};

struct Overlay {
  bool applied = false;
  RECT dirty = {0, 0, 0, 0};  // area gravada (coordenadas do buffer)
};

RECT ClampRegion(const RECT* region, int width, int height) {
  RECT r = {0, 0, width, height};
  if (region != nullptr) {
    r.left = std::max<LONG>(r.left, region->left);
    r.top = std::max<LONG>(r.top, region->top);
    r.right = std::min<LONG>(r.right, region->right);
    r.bottom = std::min<LONG>(r.bottom, region->bottom);
  }
  return r;
}

// Pede aos plugins que desenhem (nao toca no buffer do jogo).
Frame RenderOverlay(int w, int h, bool scan_box = true) {
  Frame f;
  f.w = w;
  f.h = h;
  Runtime& rt = Runtime::Get();
  if (!rt.WantsPresent() || !EnsureDib(w, h)) return f;
  const uint32_t key = AEONIAN_CANVAS_KEY & 0x00FFFFFFu;
  std::fill(g.dib, g.dib + static_cast<size_t>(w) * static_cast<size_t>(h), key);
  AeonianCanvas canvas;
  std::memset(&canvas, 0, sizeof(canvas));
  canvas.struct_size = sizeof(canvas);
  canvas.hdc = g.dc;
  canvas.width = w;
  canvas.height = h;
  canvas.opacity = 255;
  canvas.frame = ++g.frame;
  rt.DispatchPresent(&canvas);
  GdiFlush();
  f.opacity = std::min<uint32_t>(canvas.opacity, 255);
  if (!scan_box) {  // o backend GL percorre os pixels por conta propria
    f.has_overlay = true;
    return f;
  }
  f.bx0 = w;
  f.by0 = h;
  for (int y = 0; y < h; ++y) {
    const uint32_t* row = g.dib + static_cast<size_t>(y) * static_cast<size_t>(w);
    for (int x = 0; x < w; ++x) {
      if ((row[x] & 0x00FFFFFFu) == key) continue;
      f.bx0 = std::min(f.bx0, x);
      f.bx1 = std::max(f.bx1, x);
      f.by0 = std::min(f.by0, y);
      f.by1 = std::max(f.by1, y);
    }
  }
  f.has_overlay = f.bx1 >= f.bx0 && f.opacity > 0;
  return f;
}

bool WriteBmp(const std::string& path, const PixelBuffer& pb, const RECT& r) {
  const int w = static_cast<int>(r.right - r.left);
  const int h = static_cast<int>(r.bottom - r.top);
  const int row_bytes = (w * 3 + 3) & ~3;
  const uint32_t image_size = static_cast<uint32_t>(row_bytes) * static_cast<uint32_t>(h);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + image_size;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(bih);
  bih.biWidth = w;
  bih.biHeight = h;  // de baixo para cima
  bih.biPlanes = 1;
  bih.biBitCount = 24;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = image_size;
  std::vector<uint8_t> out(image_size);
  const int bytespp = pb.fmt.bits / 8;
  for (int y = 0; y < h; ++y) {
    uint8_t* dst = &out[static_cast<size_t>(h - 1 - y) * static_cast<size_t>(row_bytes)];
    const uint8_t* src = RowPtr(pb, static_cast<int>(r.top) + y) + r.left * bytespp;
    for (int x = 0; x < w; ++x) {
      const Rgb c = Unpack(pb.fmt, src + x * bytespp);
      dst[x * 3 + 0] = c.b;
      dst[x * 3 + 1] = c.g;
      dst[x * 3 + 2] = c.r;
    }
  }
  HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  const bool ok = WriteFile(file, &bfh, sizeof(bfh), &written, nullptr) &&
                  WriteFile(file, &bih, sizeof(bih), &written, nullptr) &&
                  WriteFile(file, out.data(), static_cast<DWORD>(out.size()), &written, nullptr);
  CloseHandle(file);
  return ok;
}

// Escreve o overlay em `pb` (regiao `region`), guarda o que cobriu e, se
// pedido, grava o quadro final em BMP. A paleta (8 bits) ja deve estar em g.
Overlay ApplyOverlay(PixelBuffer* pb, const RECT& region, const Frame& f,
                     const std::string& shot) {
  Overlay result;
  const int bytespp = pb->fmt.bits / 8;
  const int rx = static_cast<int>(region.left);
  const int ry = static_cast<int>(region.top);
  if (f.has_overlay) {
    const int bw = f.bx1 - f.bx0 + 1;
    const int bh = f.by1 - f.by0 + 1;
    const size_t row_len = static_cast<size_t>(bw) * static_cast<size_t>(bytespp);
    g.backup.resize(row_len * static_cast<size_t>(bh));
    for (int r = 0; r < bh; ++r) {
      std::memcpy(&g.backup[static_cast<size_t>(r) * row_len],
                  RowPtr(*pb, ry + f.by0 + r) + (rx + f.bx0) * bytespp, row_len);
    }
    const uint32_t key = AEONIAN_CANVAS_KEY & 0x00FFFFFFu;
    for (int y = f.by0; y <= f.by1; ++y) {
      const uint32_t* row = g.dib + static_cast<size_t>(y) * static_cast<size_t>(f.w);
      uint8_t* dst_row = RowPtr(*pb, ry + y);
      for (int x = f.bx0; x <= f.bx1; ++x) {
        const uint32_t px = row[x] & 0x00FFFFFFu;
        if (px == key) continue;
        Rgb c{static_cast<uint8_t>((px >> 16) & 0xFF), static_cast<uint8_t>((px >> 8) & 0xFF),
              static_cast<uint8_t>(px & 0xFF)};
        uint8_t* p = dst_row + (rx + x) * bytespp;
        if (f.opacity < 255) {
          const Rgb under = Unpack(pb->fmt, p);
          const uint32_t a = f.opacity;
          auto mix = [a](uint8_t u, uint8_t o) -> uint8_t {
            return static_cast<uint8_t>((u * (255u - a) + o * a) / 255u);
          };
          c = Rgb{mix(under.r, c.r), mix(under.g, c.g), mix(under.b, c.b)};
        }
        Pack(pb->fmt, p, c);
      }
    }
    result.applied = true;
    result.dirty = RECT{rx + f.bx0, ry + f.by0, rx + f.bx1 + 1, ry + f.by1 + 1};
  }
  if (!shot.empty()) {
    const bool ok = WriteBmp(shot, *pb, region);
    log::Writef(ok ? log::Level::kInfo : log::Level::kWarn,
                "screenshot %s: %s (%ldx%ld, %d bpp)", ok ? "gravado" : "FALHOU",
                shot.c_str(), region.right - region.left, region.bottom - region.top,
                pb->fmt.bits);
  }
  return result;
}

void RestoreOverlay(PixelBuffer* pb, const Overlay& ov) {
  if (!ov.applied || g.backup.empty()) return;
  const int bytespp = pb->fmt.bits / 8;
  const size_t row_len = static_cast<size_t>(ov.dirty.right - ov.dirty.left) * static_cast<size_t>(bytespp);
  const size_t rows = static_cast<size_t>(ov.dirty.bottom - ov.dirty.top);
  if (row_len * rows != g.backup.size()) return;
  for (size_t r = 0; r < rows; ++r) {
    std::memcpy(RowPtr(*pb, static_cast<int>(ov.dirty.top) + static_cast<int>(r)) +
                    ov.dirty.left * bytespp,
                &g.backup[r * row_len], row_len);
  }
}

bool SupportedDepth(int bits) {
  return bits == 8 || bits == 16 || bits == 24 || bits == 32;
}

// --- Backend DirectDraw ---------------------------------------------------------

bool ReadDesc(void* surface, DDSURFACEDESC* desc) {
  std::memset(desc, 0, sizeof(*desc));
  desc->dwSize = sizeof(*desc);
  return SUCCEEDED(g.get_desc(surface, desc));
}

SurfaceFormat FormatOf(const DDSURFACEDESC& d) {
  SurfaceFormat f;
  f.bits = static_cast<int>(d.ddpfPixelFormat.dwRGBBitCount);
  f.rmask = d.ddpfPixelFormat.dwRBitMask;
  f.gmask = d.ddpfPixelFormat.dwGBitMask;
  f.bmask = d.ddpfPixelFormat.dwBBitMask;
  MaskInfo(f.rmask, &f.rshift, &f.rbits);
  MaskInfo(f.gmask, &f.gshift, &f.gbits);
  MaskInfo(f.bmask, &f.bshift, &f.bbits);
  return f;
}

// Bloqueia a superficie e descreve seus pixels.
bool LockSurface(void* surface, const SurfaceFormat& fmt, bool readonly, PixelBuffer* pb) {
  DDSURFACEDESC locked;
  std::memset(&locked, 0, sizeof(locked));
  locked.dwSize = sizeof(locked);
  const DWORD flags = DDLOCK_WAIT | (readonly ? DDLOCK_READONLY : 0);
  if (FAILED(g.lock(surface, nullptr, &locked, flags, nullptr)) || locked.lpSurface == nullptr) {
    return false;
  }
  pb->base = static_cast<uint8_t*>(locked.lpSurface);
  pb->pitch = static_cast<int>(locked.lPitch);
  pb->width = static_cast<int>(locked.dwWidth);
  pb->height = static_cast<int>(locked.dwHeight);
  pb->fmt = fmt;
  return true;
}

// Desenha o overlay em `target` antes de o quadro ser apresentado. Devolve o
// que precisa ser restaurado depois (so no Blt; no Flip o buffer e repintado).
Overlay DdrawBefore(void* target, const RECT* region_in) {
  Overlay none;
  Runtime& rt = Runtime::Get();
  const std::string shot = rt.TakeScreenshotRequest();
  if (!rt.WantsPresent() && shot.empty()) return none;

  DDSURFACEDESC desc;
  if (!ReadDesc(target, &desc)) return none;
  const SurfaceFormat fmt = FormatOf(desc);
  if (!SupportedDepth(fmt.bits)) return none;
  const RECT region = ClampRegion(region_in, static_cast<int>(desc.dwWidth),
                                  static_cast<int>(desc.dwHeight));
  const int w = static_cast<int>(region.right - region.left);
  const int h = static_cast<int>(region.bottom - region.top);
  if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return none;

  if (fmt.bits == 8) RefreshPaletteDdraw();
  const Frame frame = RenderOverlay(w, h);
  if (!frame.has_overlay && shot.empty()) return none;

  PixelBuffer pb;
  if (!LockSurface(target, fmt, frame.has_overlay ? false : true, &pb)) return none;
  const Overlay ov = ApplyOverlay(&pb, region, frame, shot);
  g.unlock(target, pb.base);
  return ov;
}

void DdrawAfter(void* target, const Overlay& ov) {
  if (!ov.applied) return;
  DDSURFACEDESC desc;
  if (!ReadDesc(target, &desc)) return;
  PixelBuffer pb;
  if (!LockSurface(target, FormatOf(desc), false, &pb)) return;
  RestoreOverlay(&pb, ov);
  g.unlock(target, pb.base);
}

struct ScopedBusy {
  ScopedBusy() { entered = InterlockedCompareExchange(&g.busy, 1, 0) == 0; }
  ~ScopedBusy() {
    if (entered) InterlockedExchange(&g.busy, 0);
  }
  bool entered = false;
};

HRESULT WINAPI HookBlt(void* self, RECT* dst, void* src, RECT* src_rect, DWORD flags,
                       void* fx) {
  if (g.logged_blts < 3 && self == g.primary) {
    ++g.logged_blts;
    log::Writef(log::Level::kDebug, "primary->Blt dst=%s src=%p src_rect=%s flags=0x%lx",
                dst != nullptr ? "rect" : "NULL", src, src_rect != nullptr ? "rect" : "NULL",
                static_cast<unsigned long>(flags));
  }
  Overlay ov;
  ScopedBusy busy;
  if (busy.entered && self == g.primary && src != nullptr && src != self) {
    ov = DdrawBefore(src, src_rect);
  }
  const HRESULT hr = g.blt(self, dst, src, src_rect, flags, fx);
  if (ov.applied) DdrawAfter(src, ov);
  return hr;
}

HRESULT WINAPI HookFlip(void* self, void* target, DWORD flags) {
  ScopedBusy busy;
  if (busy.entered && self == g.primary) {
    void* back = target;
    bool release_back = false;
    if (back == nullptr && g.get_attached != nullptr) {
      DDSCAPS caps = {};
      caps.dwCaps = DDSCAPS_BACKBUFFER;
      if (SUCCEEDED(g.get_attached(self, &caps, &back)) && back != nullptr) release_back = true;
    }
    if (back != nullptr) DdrawBefore(back, nullptr);  // apos o flip o buffer e repintado
    if (release_back && back != nullptr) Slot<ReleaseFn>(VtableOf(back), kSurfRelease)(back);
  }
  return g.flip(self, target, flags);
}

HRESULT WINAPI HookCreateSurface(void* self, DDSURFACEDESC* desc, void** out, IUnknown* outer) {
  const HRESULT hr = g.create_surface(self, desc, out, outer);
  if (FAILED(hr) || out == nullptr || *out == nullptr || desc == nullptr) return hr;

  const bool primary = (desc->dwFlags & DDSD_CAPS) != 0 &&
                       (desc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) != 0;
  void** vt = VtableOf(*out);
  if (!g.surface_patched) {
    g.get_desc = Slot<GetDescFn>(vt, kSurfGetSurfaceDesc);
    g.get_palette = Slot<GetPaletteFn>(vt, kSurfGetPalette);
    g.get_attached = Slot<GetAttachedFn>(vt, kSurfGetAttachedSurface);
    g.lock = Slot<LockFn>(vt, kSurfLock);
    g.unlock = Slot<UnlockFn>(vt, kSurfUnlock);
    const bool ok = PatchSlot<BltFn>(vt, kSurfBlt, &HookBlt, &g.blt) &&
                    PatchSlot<FlipFn>(vt, kSurfFlip, &HookFlip, &g.flip);
    g.surface_patched = ok;
    log::Writef(ok ? log::Level::kInfo : log::Level::kError,
                "DirectDraw: metodos Blt/Flip das superficies %s",
                ok ? "interceptados" : "NAO puderam ser interceptados");
  }
  if (primary) {
    g.primary = *out;
    log::Writef(log::Level::kInfo, "DirectDraw: superficie primaria criada (%p)", *out);
  }
  if (log::GetLevel() <= log::Level::kDebug) {
    DDSURFACEDESC d;
    if (ReadDesc(*out, &d)) {
      log::Writef(log::Level::kDebug, "DirectDraw: superficie %p caps=0x%lx %lux%lu %lu bpp", *out,
                  static_cast<unsigned long>(desc->ddsCaps.dwCaps),
                  static_cast<unsigned long>(d.dwWidth), static_cast<unsigned long>(d.dwHeight),
                  static_cast<unsigned long>(d.ddpfPixelFormat.dwRGBBitCount));
    }
  }
  return hr;
}

}  // namespace

// --- Backend GDI (StretchBlt) -------------------------------------------------------

namespace {

// O renderizador GDI do jogo desenha um quadro de 8 bits num DIB (memory DC) e o
// envia a janela com StretchBlt. Aqui reconhecemos esse DIB e compomos nele.
bool DibOfDc(HDC dc, PixelBuffer* pb, BITMAPINFOHEADER* header_out) {
  HGDIOBJ bmp = GetCurrentObject(dc, OBJ_BITMAP);
  if (bmp == nullptr) return false;
  DIBSECTION ds;
  if (GetObjectA(bmp, sizeof(ds), &ds) != static_cast<int>(sizeof(ds))) return false;
  if (ds.dsBm.bmBits == nullptr || ds.dsBmih.biCompression != BI_RGB) return false;
  const int bits = ds.dsBm.bmBitsPixel;
  if (!SupportedDepth(bits)) return false;
  const int w = ds.dsBm.bmWidth;
  const int h = ds.dsBm.bmHeight;  // GetObject ja devolve altura positiva
  const int stride = ds.dsBm.bmWidthBytes;
  bool bottom_up = ds.dsBmih.biHeight > 0;  // so se o registro nao existir
  const char* origin = "GetObject";
  if (g.dib_lock_ready) {
    EnterCriticalSection(&g.dib_lock);
    for (const Video::DibRecord& r : g.dibs) {
      if (r.bits == ds.dsBm.bmBits) {
        bottom_up = !r.top_down;
        origin = "CreateDIBSection";
      }
    }
    LeaveCriticalSection(&g.dib_lock);
  }
  if (g.logged_gdi < 2) {
    log::Writef(log::Level::kInfo, "GDI: DIB %dx%d, orientacao %s (origem: %s)", w, h,
                bottom_up ? "de baixo para cima" : "de cima para baixo", origin);
  }
  pb->width = w;
  pb->height = h;
  pb->fmt = SurfaceFormat{};
  pb->fmt.bits = bits;
  if (bits == 16) {  // BI_RGB de 16 bits = 5-5-5
    pb->fmt.rmask = 0x7C00; pb->fmt.gmask = 0x03E0; pb->fmt.bmask = 0x001F;
  } else if (bits == 32) {
    pb->fmt.rmask = 0xFF0000; pb->fmt.gmask = 0x00FF00; pb->fmt.bmask = 0x0000FF;
  }
  MaskInfo(pb->fmt.rmask, &pb->fmt.rshift, &pb->fmt.rbits);
  MaskInfo(pb->fmt.gmask, &pb->fmt.gshift, &pb->fmt.gbits);
  MaskInfo(pb->fmt.bmask, &pb->fmt.bshift, &pb->fmt.bbits);
  uint8_t* bits_ptr = static_cast<uint8_t*>(ds.dsBm.bmBits);
  if (bottom_up) {
    pb->base = bits_ptr + static_cast<ptrdiff_t>(h - 1) * stride;
    pb->pitch = -stride;
  } else {
    pb->base = bits_ptr;
    pb->pitch = stride;
  }
  if (header_out != nullptr) *header_out = ds.dsBmih;
  return true;
}

void RefreshPaletteGdi(HDC dc) {
  RGBQUAD colors[256];
  if (GetDIBColorTable(dc, 0, 256, colors) != 256) return;
  PALETTEENTRY now[256];
  for (int i = 0; i < 256; ++i) {
    now[i].peRed = colors[i].rgbRed;
    now[i].peGreen = colors[i].rgbGreen;
    now[i].peBlue = colors[i].rgbBlue;
    now[i].peFlags = 0;
  }
  if (std::memcmp(now, g.palette, sizeof(now)) != 0) {
    std::memcpy(g.palette, now, sizeof(now));
    g.nearest.assign(32768, -1);
  }
}

}  // namespace

HBITMAP WINAPI HookCreateDIBSection(HDC dc, const BITMAPINFO* info, UINT usage, void** bits,
                                    HANDLE section, DWORD offset) {
  HBITMAP bitmap = g_real_create_dib_section(dc, info, usage, bits, section, offset);
  if (bitmap != nullptr && info != nullptr && bits != nullptr && *bits != nullptr &&
      g.dib_lock_ready) {
    Video::DibRecord rec = {*bits, info->bmiHeader.biHeight < 0,
                            static_cast<int>(info->bmiHeader.biWidth),
                            static_cast<int>(std::abs(info->bmiHeader.biHeight)),
                            static_cast<int>(info->bmiHeader.biBitCount)};
    EnterCriticalSection(&g.dib_lock);
    for (size_t i = 0; i < g.dibs.size(); ++i) {  // ponteiro reaproveitado: substitui
      if (g.dibs[i].bits == rec.bits) {
        g.dibs.erase(g.dibs.begin() + static_cast<ptrdiff_t>(i));
        break;
      }
    }
    if (g.dibs.size() >= 64) g.dibs.erase(g.dibs.begin());
    g.dibs.push_back(rec);
    LeaveCriticalSection(&g.dib_lock);
  }
  return bitmap;
}

BOOL WINAPI HookStretchBlt(HDC dst, int xd, int yd, int wd, int hd, HDC src, int xs, int ys,
                           int ws, int hs, DWORD rop) {
  Overlay ov;
  PixelBuffer pb;
  bool active = false;
  ScopedBusy busy;
  Runtime& rt = Runtime::Get();
  // So o quadro do jogo: origem = DIB em memoria >= 320x200, destino = janela.
  if (busy.entered && src != nullptr && ws >= 320 && hs >= 200 &&
      GetObjectType(dst) != OBJ_MEMDC && GetObjectType(src) == OBJ_MEMDC &&
      (rt.WantsPresent() || rt.HasScreenshotRequest()) && DibOfDc(src, &pb, nullptr)) {
    GdiFlush();
    const std::string shot = rt.TakeScreenshotRequest();
    if (pb.fmt.bits == 8) RefreshPaletteGdi(src);
    const RECT want = {xs, ys, xs + ws, ys + hs};
    const RECT region = ClampRegion(&want, pb.width, pb.height);
    const int w = static_cast<int>(region.right - region.left);
    const int h = static_cast<int>(region.bottom - region.top);
    if (w > 0 && h > 0 && w <= 4096 && h <= 4096) {
      const Frame frame = RenderOverlay(w, h);
      if (frame.has_overlay || !shot.empty()) {
        ov = ApplyOverlay(&pb, region, frame, shot);
        active = true;
      }
    }
    if (g.logged_gdi < 2) {
      ++g.logged_gdi;
      log::Writef(log::Level::kInfo,
                  "GDI: quadro %dx%d %d bpp via StretchBlt (origem %d,%d %dx%d -> destino %dx%d)",
                  pb.width, pb.height, pb.fmt.bits, xs, ys, ws, hs, wd, hd);
    }
  }
  const BOOL result = g_real_stretch_blt(dst, xd, yd, wd, hd, src, xs, ys, ws, hs, rop);
  if (active) RestoreOverlay(&pb, ov);
  return result;
}

bool RenderPluginCanvas(int width, int height, CanvasPixels* out) {
  if (width <= 0 || height <= 0 || width > 8192 || height > 8192) return false;
  const Frame f = RenderOverlay(width, height, /*scan_box=*/false);
  if (g.dib == nullptr || g.dib_w != width || g.dib_h != height) return false;
  out->bits = g.dib;
  out->width = width;
  out->height = height;
  out->opacity = f.opacity;
  return true;
}

void InitVideo() {
  if (g.dib_lock_ready) return;
  InitializeCriticalSection(&g.dib_lock);
  g.dib_lock_ready = true;
}

HRESULT WINAPI HookDirectDrawCreate(GUID* guid, void** out, IUnknown* outer) {
  const HRESULT hr = g_real_direct_draw_create(guid, out, outer);
  if (SUCCEEDED(hr) && out != nullptr && *out != nullptr && !g.dd_patched) {
    void** vt = VtableOf(*out);
    g.dd_patched = PatchSlot<CreateSurfaceFn>(vt, kDdCreateSurface, &HookCreateSurface,
                                              &g.create_surface);
    log::Writef(g.dd_patched ? log::Level::kInfo : log::Level::kError,
                "DirectDrawCreate interceptado: CreateSurface %s",
                g.dd_patched ? "ok" : "FALHOU");
  }
  return hr;
}

}  // namespace aeonian
