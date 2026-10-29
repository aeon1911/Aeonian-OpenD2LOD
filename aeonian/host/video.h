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

// aeonian/host/video.h
//
// Ponto de apresentacao de quadro (on_present), sem nenhum endereco interno do
// jogo. O Game.exe tem dois renderizadores de software, ambos por importacoes:
//  * GDI: um DIB de 8 bits enviado a janela por StretchBlt;
//  * DirectDraw: DirectDrawCreate; interceptamos pelas tabelas de metodos COM
//    CreateSurface/Blt/Flip. A cada apresentacao o host entrega ao plugin uma
// tela GDI, compoe o resultado na superficie do jogo (convertendo para a
// paleta quando preciso) e restaura os pixels originais.

#ifndef AEONIAN_HOST_VIDEO_H_
#define AEONIAN_HOST_VIDEO_H_

#include <windows.h>

#include <cstdint>

namespace aeonian {

using DirectDrawCreateFn = HRESULT(WINAPI*)(GUID*, void**, IUnknown*);

// Substituto da importacao DDRAW.dll!DirectDrawCreate; `g_real_direct_draw_create`
// recebe a original quando o hook de IAT e instalado.
HRESULT WINAPI HookDirectDrawCreate(GUID* guid, void** out, IUnknown* outer);
extern DirectDrawCreateFn g_real_direct_draw_create;

// Modo GDI (renderizador de software do jogo): quadro de 8 bits em um DIB
// enviado a janela por GDI32!StretchBlt.
using StretchBltFn = BOOL(WINAPI*)(HDC, int, int, int, int, HDC, int, int, int, int, DWORD);
BOOL WINAPI HookStretchBlt(HDC dst, int xd, int yd, int wd, int hd, HDC src, int xs, int ys,
                           int ws, int hs, DWORD rop);
extern StretchBltFn g_real_stretch_blt;

// O jogo cria o DIB do quadro com CreateDIBSection; guardamos a orientacao
// (de cima para baixo ou de baixo para cima) com que ele o criou, porque a
// altura devolvida por GetObject nao e confiavel entre implementacoes.
using CreateDIBSectionFn = HBITMAP(WINAPI*)(HDC, const BITMAPINFO*, UINT, void**, HANDLE, DWORD);
HBITMAP WINAPI HookCreateDIBSection(HDC dc, const BITMAPINFO* info, UINT usage, void** bits,
                                    HANDLE section, DWORD offset);
extern CreateDIBSectionFn g_real_create_dib_section;

// Prepara o estado do modulo (chamar antes de instalar os hooks).
void InitVideo();

// Pede aos plugins (on_present) que desenhem num canvas width x height e devolve
// o DIB BGRA 32 bits onde desenharam (cor-chave = transparente).
struct CanvasPixels {
  uint32_t* bits = nullptr;
  int width = 0;
  int height = 0;
  uint32_t opacity = 255;
};
bool RenderPluginCanvas(int width, int height, CanvasPixels* out);

// --- Modo D2GL (OpenGL) ---------------------------------------------------------
// O D2GL e um glide3x.dll que o jogo carrega com -3dfx e que apresenta cada
// quadro com GDI32!SwapBuffers. Ao detectar o carregamento do modulo, trocamos
// essa importacao para compor o overlay dos plugins em OpenGL antes do swap.
using LoadLibraryAFn = HMODULE(WINAPI*)(LPCSTR);
HMODULE WINAPI HookLoadLibraryA(LPCSTR name);
extern LoadLibraryAFn g_real_load_library_a;

}  // namespace aeonian

#endif  // AEONIAN_HOST_VIDEO_H_
