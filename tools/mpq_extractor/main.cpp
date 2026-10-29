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

// ae_mpq_extract — extrator de arquivos MPQ usando StormLib (MIT).
//
// Uso:
//   ae_mpq_extract <arquivo.mpq> list
//   ae_mpq_extract <arquivo.mpq> extract <diretorio_de_saida> [prefixo]
//
// O "prefixo" filtra entradas por inicio do caminho (ex.: "global/excel").
// Nao coloque a saida dentro do repositorio (protocolo cleanroom).

#include <StormLib.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

namespace fs = std::filesystem;

// Converte separadores internos do MPQ ('\\') para o separador nativo.
fs::path ToNativePath(const char* mpq_path) {
    std::string normalized(mpq_path);
    for (char& c : normalized) {
        if (c == '\\') {
            c = static_cast<char>(fs::path::preferred_separator);
        }
    }
    return fs::path(normalized);
}

bool HasPrefix(const std::string& value, const char* prefix) {
    return std::strncmp(value.c_str(), prefix, std::strlen(prefix)) == 0;
}

int CmdList(HANDLE mpq) {
    SFILE_FIND_DATA entry{};
    HANDLE find = SFileFindFirstFile(mpq, "*", &entry, nullptr);
    if (find == nullptr) {
        std::fprintf(stderr, "Falha ao enumerar o MPQ (codigo %u).\n",
                     static_cast<unsigned>(SErrGetLastError()));
        return 1;
    }
    std::size_t count = 0;
    do {
        std::printf("%s\n", entry.cFileName);
        ++count;
    } while (SFileFindNextFile(find, &entry));
    SFileFindClose(find);
    std::printf("-- %zu arquivos --\n", count);
    return 0;
}

int CmdExtract(HANDLE mpq, const fs::path& out_dir, const char* prefix) {
    SFILE_FIND_DATA entry{};
    HANDLE find = SFileFindFirstFile(mpq, "*", &entry, nullptr);
    if (find == nullptr) {
        std::fprintf(stderr, "Falha ao enumerar o MPQ (codigo %u).\n",
                     static_cast<unsigned>(SErrGetLastError()));
        return 1;
    }

    std::size_t ok = 0;
    std::size_t skipped = 0;
    std::size_t failed = 0;
    do {
        const std::string name(entry.cFileName);
        if (prefix != nullptr && !HasPrefix(name, prefix)) {
            ++skipped;
            continue;
        }
        const fs::path dest = out_dir / ToNativePath(name.c_str());
        if (dest.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(dest.parent_path(), ec);
        }
        if (SFileExtractFile(mpq, name.c_str(), dest.string().c_str(),
                              0 /* do arquivo original, nao de cache */)) {
            ++ok;
        } else {
            std::fprintf(stderr, "Falha ao extrair: %s (codigo %u)\n",
                         name.c_str(), static_cast<unsigned>(SErrGetLastError()));
            ++failed;
        }
    } while (SFileFindNextFile(find, &entry));
    SFileFindClose(find);

    std::printf("extraidos=%zu pulados=%zu falhas=%zu\n", ok, skipped, failed);
    return failed == 0 ? 0 : 1;
}

void PrintUsage() {
    std::fprintf(stderr,
                 "Uso:\n"
                 "  ae_mpq_extract <arquivo.mpq> list\n"
                 "  ae_mpq_extract <arquivo.mpq> extract <dir_saida> [prefixo]\n");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        PrintUsage();
        return 2;
    }
    const char* mpq_path = argv[1];
    const char* command = argv[2];

    HANDLE mpq = nullptr;
    if (!SFileOpenArchive(mpq_path, 0, STREAM_FLAG_READ_ONLY, &mpq)) {
        std::fprintf(stderr, "Falha ao abrir o MPQ '%s' (codigo %u).\n",
                     mpq_path, static_cast<unsigned>(SErrGetLastError()));
        return 1;
    }

    int result = 0;
    if (std::strcmp(command, "list") == 0) {
        result = CmdList(mpq);
    } else if (std::strcmp(command, "extract") == 0 && argc >= 4) {
        const char* prefix = (argc >= 5) ? argv[4] : nullptr;
        result = CmdExtract(mpq, fs::path(argv[3]), prefix);
    } else {
        PrintUsage();
        result = 2;
    }

    SFileCloseArchive(mpq);
    return result;
}
