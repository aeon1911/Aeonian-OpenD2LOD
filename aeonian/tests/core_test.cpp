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

// Testes do nucleo portavel do Aeonian (ini, vfs, pe_image, manifesto,
// banco de enderecos, linha de comando). Rodam no host; nao tocam no jogo.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/address_db.h"
#include "core/cmdline.h"
#include "core/ini.h"
#include "core/mod_manifest.h"
#include "core/pe_image.h"
#include "core/vfs.h"

namespace {

int g_failures = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "%s:%d: CHECK falhou: %s\n", __FILE__, __LINE__, \
                   #cond);                                                \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b)                                                    \
  do {                                                                    \
    auto va_ = (a);                                                       \
    auto vb_ = (b);                                                       \
    if (!(va_ == vb_)) {                                                  \
      std::fprintf(stderr, "%s:%d: CHECK_EQ falhou: %s != %s\n", __FILE__, \
                   __LINE__, #a, #b);                                     \
      ++g_failures;                                                       \
    }                                                                     \
  } while (0)

using namespace aeonian;

void TestIni() {
  Ini ini = Ini::Parse(
      "\xEF\xBB\xBF; comentario\n"
      "[Core]\r\n"
      "Log_Level = debug   ; inline\n"
      "force_direct=auto\n"
      "name = \"a ; b\"\n"
      "n = 0x10\n"
      "flag = yes\n"
      "[mods]\n"
      "disabled = a, b  c\n"
      "[core]\n"
      "extra = 1\n");
  CHECK_EQ(ini.GetOr("CORE", "LOG_LEVEL", ""), "debug");
  CHECK_EQ(ini.GetOr("core", "force_direct", ""), "auto");
  CHECK_EQ(ini.GetOr("core", "name", ""), "a ; b");  // aspas preservam
  CHECK_EQ(ini.GetInt("core", "n", 0), 16);
  CHECK(ini.GetBool("core", "flag", false));
  CHECK(!ini.GetBool("core", "absent", false));
  CHECK_EQ(ini.GetOr("core", "extra", ""), "1");  // secao repetida mescla
  CHECK_EQ(ini.SectionNames().size(), 2u);
  CHECK_EQ(SplitList(ini.GetOr("mods", "disabled", "")).size(), 3u);

  CHECK_EQ(*ParseInt("-12"), -12);
  CHECK_EQ(*ParseInt("0xFF"), 255);
  CHECK(!ParseInt("").has_value());
  CHECK(!ParseInt("12x").has_value());
  CHECK(!ParseInt("0x").has_value());
  CHECK(!ParseInt("99999999999999999999").has_value());  // estouro
}

void TestVfs() {
  std::set<std::string> files = {
      "C:\\Jogo\\aeonian\\mods\\hud\\data\\global\\ui\\panel.dc6",
      "C:\\Jogo\\aeonian\\mods\\base\\data\\global\\ui\\panel.dc6",
      "C:\\Jogo\\aeonian\\mods\\base\\data\\global\\excel\\skills.txt",
  };
  // O Windows (e o Wine) nao diferenciam maiusculas em nomes de arquivo: o
  // Vfs preserva a caixa do pedido e deixa o SO decidir.
  Vfs vfs("C:/Jogo/", {{"hud", "C:\\Jogo\\aeonian\\mods\\hud"},
                       {"base", "C:\\Jogo\\aeonian\\mods\\base/"}},
          [&](const std::string& p) {
            for (const std::string& f : files) {
              if (ToLowerAscii(f) == ToLowerAscii(p)) return true;
            }
            return false;
          });

  std::string owner;
  // Absoluto, barras mistas, caixa diferente.
  auto r = vfs.Resolve("c:/jogo\\DATA\\global/UI\\panel.dc6", &owner);
  CHECK(r.has_value());
  CHECK_EQ(ToLowerAscii(*r),
           "c:\\jogo\\aeonian\\mods\\hud\\data\\global\\ui\\panel.dc6");
  CHECK_EQ(owner, "hud");  // maior prioridade vence

  // So o mod de menor prioridade fornece este.
  r = vfs.Resolve("C:\\Jogo\\data\\global\\excel\\skills.txt", &owner);
  CHECK(r.has_value());
  CHECK_EQ(owner, "base");

  // Relativo a pasta do jogo, com "." e barras duplas.
  r = vfs.Resolve(".\\data\\\\global\\.\\ui\\panel.dc6");
  CHECK(r.has_value());

  // Ninguem fornece: passa direto.
  CHECK(!vfs.Resolve("C:\\Jogo\\data\\global\\ui\\none.dc6").has_value());
  // Fora de data\.
  CHECK(!vfs.Resolve("C:\\Jogo\\d2data.mpq").has_value());
  CHECK(!vfs.Resolve("C:\\Jogo\\save\\hero.d2s").has_value());
  // Outra pasta de jogo com prefixo parecido.
  CHECK(!vfs.Resolve("C:\\JogoX\\data\\global\\ui\\panel.dc6").has_value());
  // Fuga por "..": recusada.
  CHECK(!vfs.Resolve("C:\\Jogo\\data\\..\\data\\global\\ui\\panel.dc6")
             .has_value());
  CHECK(!vfs.Resolve("data\\..\\..\\x").has_value());
  // Curinga e diretorio raiz de data.
  CHECK(!vfs.Resolve("C:\\Jogo\\data\\global\\ui\\*.dc6").has_value());
  CHECK(!vfs.Resolve("C:\\Jogo\\data").has_value());
  CHECK(!vfs.Resolve("C:\\Jogo\\data\\").has_value());
  // O proprio caminho de um mod nao e redirecionado de novo.
  CHECK(!vfs.Resolve("C:\\Jogo\\aeonian\\mods\\hud\\data\\global\\ui\\panel.dc6")
             .has_value());

  CHECK_EQ(*vfs.RelativeDataPath("data\\global\\x.txt"), "global\\x.txt");
  CHECK_EQ(NormalizeDir("C:/a/b//"), "C:\\a\\b");
}

// Monta uma imagem PE32 minima ja mapeada com uma importacao
// KERNEL32.dll -> {Sleep, CreateFileA}, para testar a IAT.
std::vector<uint8_t> BuildFakePe(uint32_t* create_file_slot_rva) {
  std::vector<uint8_t> img(0x2000, 0);
  auto w32 = [&](size_t off, uint32_t v) { std::memcpy(&img[off], &v, 4); };
  auto w16 = [&](size_t off, uint16_t v) { std::memcpy(&img[off], &v, 2); };
  w16(0, 0x5A4D);
  w32(0x3C, 0x80);
  w32(0x80, 0x4550);
  w16(0x84, 0x014C);       // machine i386
  w32(0x88, 0x574ddfbc);   // timestamp
  const size_t opt = 0x80 + 24;
  w16(opt, 0x010B);
  w32(opt + 16, 0x1234);   // entry
  w32(opt + 28, 0x400000); // image base
  w32(opt + 56, 0x2000);   // size of image
  w32(opt + 64, 0xABCD);   // checksum
  w32(opt + 96 + 8, 0x1000);  // import dir rva
  w32(opt + 96 + 12, 40);
  // descritor 0 @0x1000; thunks @0x1100 (original) e @0x1200 (IAT).
  w32(0x1000 + 0, 0x1100);
  w32(0x1000 + 12, 0x1300);  // nome da DLL
  w32(0x1000 + 16, 0x1200);
  std::memcpy(&img[0x1300], "KERNEL32.dll", 13);
  w32(0x1100, 0x1400);  // Sleep
  w32(0x1104, 0x1420);  // CreateFileA
  w32(0x1108, 0x80000010u);  // por ordinal
  w32(0x1200, 0xDEAD0001);
  w32(0x1204, 0xDEAD0002);
  std::memcpy(&img[0x1402], "Sleep", 6);        // hint de 2 bytes antes
  std::memcpy(&img[0x1422], "CreateFileA", 12);
  *create_file_slot_rva = 0x1204;
  return img;
}

void TestPe() {
  uint32_t slot_rva = 0;
  std::vector<uint8_t> img = BuildFakePe(&slot_rva);
  PeInfo pe;
  CHECK(ParsePeHeaders(img.data(), img.size(), &pe));
  CHECK_EQ(pe.timestamp, 0x574ddfbcu);
  CHECK_EQ(pe.size_of_image, 0x2000u);
  CHECK_EQ(pe.entry_rva, 0x1234u);
  CHECK_EQ(pe.image_base, 0x400000u);
  CHECK_EQ(pe.checksum, 0xABCDu);

  uint32_t* slot = FindImportSlot(img.data(), img.size(), "kernel32.DLL",
                                  "CreateFileA");
  CHECK(slot != nullptr);
  if (slot != nullptr) {
    CHECK_EQ(reinterpret_cast<uint8_t*>(slot) - img.data(),
             static_cast<ptrdiff_t>(slot_rva));
    CHECK_EQ(*slot, 0xDEAD0002u);
  }
  CHECK(FindImportSlot(img.data(), img.size(), "KERNEL32.dll", "Sleep") !=
        nullptr);
  CHECK(FindImportSlot(img.data(), img.size(), "KERNEL32.dll", "Nope") ==
        nullptr);
  CHECK(FindImportSlot(img.data(), img.size(), "USER32.dll", "Sleep") ==
        nullptr);

  // Imagem truncada/corrompida nunca causa leitura fora dos limites.
  CHECK(!ParsePeHeaders(img.data(), 0x40, &pe));
  CHECK(FindImportSlot(img.data(), 0x1010, "KERNEL32.dll", "CreateFileA") ==
        nullptr);
  std::vector<uint8_t> bad = img;
  bad[0] = 'X';
  CHECK(!ParsePeHeaders(bad.data(), bad.size(), &pe));
  bad = img;
  uint16_t machine = 0x8664;  // x64: recusado
  std::memcpy(&bad[0x84], &machine, 2);
  CHECK(!ParsePeHeaders(bad.data(), bad.size(), &pe));
}

void TestManifest() {
  std::string err;
  auto m = ParseModManifest(
      "[mod]\nname = Meu HUD\npriority = 50\nplugin = hud.dll, sub/b.dll\n"
      "args = -txt -foo\nbuilds = 1.14D\n",
      "meu-hud", &err);
  CHECK(m.has_value());
  CHECK_EQ(m->id, "meu-hud");  // veio do nome da pasta
  CHECK_EQ(m->priority, 50);
  CHECK_EQ(m->plugins.size(), 2u);
  CHECK_EQ(m->args.size(), 2u);
  CHECK(SupportsBuild(*m, "1.14d"));
  CHECK(!SupportsBuild(*m, "1.13c"));

  CHECK(!ParseModManifest("[mod]\nplugin = ..\\evil.dll\n", "x", &err));
  CHECK(!ParseModManifest("[mod]\nplugin = C:\\evil.dll\n", "x", &err));
  CHECK(!ParseModManifest("[mod]\nplugin = /etc/x.so\n", "x", &err));
  CHECK(!ParseModManifest("[mod]\nargs = direct\n", "x", &err));
  CHECK(!ParseModManifest("[mod]\nid = Bad Id!\n", "x", &err));
  CHECK(!ParseModManifest("[mod]\npriority = 999999\n", "x", &err));
  CHECK(!ParseModManifest("", "Pasta Com Espaco", &err));  // id da pasta invalido
  auto empty = ParseModManifest("", "ok", &err);
  CHECK(empty.has_value());
  CHECK(SupportsBuild(*empty, "qualquer"));

  std::vector<ModManifest> mods(3);
  mods[0].id = "b"; mods[0].priority = 1;
  mods[1].id = "a"; mods[1].priority = 1;
  mods[2].id = "z"; mods[2].priority = 9;
  SortForLoad(&mods);
  CHECK_EQ(mods[0].id, "z");
  CHECK_EQ(mods[1].id, "a");
  CHECK_EQ(mods[2].id, "b");
}

void TestAddressDb() {
  std::string err;
  auto db = AddressDb::Parse(
      "[build]\nname=t\nid=7\ntimestamp=0x10\nsize_of_image=0x20\n"
      "entry_rva=0x30\n[symbol.Foo.Bar]\nrva=0x100\nbytes=e8 1b 9F\n",
      &err);
  CHECK(db.has_value());
  PeInfo pe;
  pe.timestamp = 0x10; pe.size_of_image = 0x20; pe.entry_rva = 0x30;
  CHECK(db->Matches(pe));
  pe.timestamp = 0x11;
  CHECK(!db->Matches(pe));
  const Symbol* s = db->Find("foo.BAR");
  CHECK(s != nullptr);
  if (s != nullptr) {
    CHECK_EQ(s->name, "Foo.Bar");
    CHECK_EQ(s->rva, 0x100u);
    CHECK_EQ(s->expect.size(), 3u);
    CHECK_EQ(s->expect[2], 0x9Fu);
  }
  CHECK(db->Find("nope") == nullptr);

  CHECK(!AddressDb::Parse("[symbol.X]\nrva=1\n", &err));          // sem [build]
  CHECK(!AddressDb::Parse("[build]\nname=t\n", &err));            // faltam campos
  CHECK(!AddressDb::Parse("[build]\nname=t\ntimestamp=1\nsize_of_image=1\n"
                          "entry_rva=1\n[symbol.A]\nbytes=zz\nrva=1\n", &err));
  CHECK(!AddressDb::Parse("[build]\nname=t\ntimestamp=1\nsize_of_image=1\n"
                          "entry_rva=1\n[symbol.A]\nbytes=e8\n", &err));  // sem rva
  CHECK(!ParseHexBytes("e8 1").has_value());  // nibble solto
  CHECK(ParseHexBytes("").has_value());

  // O banco real da 1.14d versionado no repositorio.
  std::ifstream f(std::string(AEONIAN_SOURCE_DIR) + "/addresses/1.14d.ini",
                  std::ios::binary);
  CHECK(f.good());
  std::stringstream buf;
  buf << f.rdbuf();
  auto real = AddressDb::Parse(buf.str(), &err);
  CHECK(real.has_value());
  if (real.has_value()) {
    CHECK_EQ(real->build_name(), "1.14d");
    CHECK_EQ(real->build_id(), 1u);
    CHECK_EQ(real->sha256().size(), 64u);
    const Symbol* ep = real->Find("Game.EntryPoint");
    CHECK(ep != nullptr);
    if (ep != nullptr) {
      CHECK_EQ(ep->rva, 0x282985u);
      CHECK_EQ(ep->expect.size(), 10u);
    }
    PeInfo game;
    game.timestamp = 0x574ddfbc; game.size_of_image = 0x5ba000;
    game.entry_rva = 0x282985;
    CHECK(real->Matches(game));
  }
}

void TestCmdline() {
  CHECK_EQ(BuildCommandLine("Game.exe -w", {"-direct"}), "Game.exe -w -direct");
  CHECK_EQ(BuildCommandLine("Game.exe -DIRECT -w", {"-direct", "-txt"}),
           "Game.exe -DIRECT -w -txt");  // nao duplica, ignora a caixa
  CHECK_EQ(BuildCommandLine("", {"-direct"}), "-direct");
  CHECK_EQ(BuildCommandLine("Game.exe ", {"-a"}), "Game.exe -a");
  CHECK_EQ(BuildCommandLine("Game.exe -w", {}), "Game.exe -w");
  // Remocao de flags (ex.: -w e incompativel com o D2GL).
  CHECK_EQ(BuildCommandLine("\"C:\\D2\\Game.exe\" -w -ns", {"-3dfx"}, {"-w"}),
           "\"C:\\D2\\Game.exe\" -ns -3dfx");
  CHECK_EQ(BuildCommandLine("Game.exe -W -w", {}, {"-w"}), "Game.exe");
  CHECK_EQ(BuildCommandLine("Game.exe -ww -ns", {}, {"-w"}), "Game.exe -ww -ns");
  // `remove` vence: a flag nao volta nem quando vem de `extra`.
  CHECK_EQ(BuildCommandLine("Game.exe -w", {"-w", "-ns"}, {"-w"}), "Game.exe -ns");
  CHECK_EQ(BuildCommandLine("Game.exe", {"-W", "-3dfx"}, {"-w"}), "Game.exe -3dfx");
  // Token parcial nao conta como presente.
  CHECK_EQ(BuildCommandLine("Game.exe -directx", {"-direct"}),
           "Game.exe -directx -direct");
}

}  // namespace

int main() {
  TestIni();
  TestVfs();
  TestPe();
  TestManifest();
  TestAddressDb();
  TestCmdline();
  if (g_failures != 0) {
    std::fprintf(stderr, "%d falha(s)\n", g_failures);
    return 1;
  }
  std::puts("aeonian core_test: OK");
  return 0;
}
