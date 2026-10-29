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

// aeonian/host/log.h
//
// Log em arquivo (aeonian/aeonian.log), uma linha por evento, seguro entre
// threads. Sem arquivo aberto, cai para OutputDebugString.

#ifndef AEONIAN_HOST_LOG_H_
#define AEONIAN_HOST_LOG_H_

#include <string>
#include <string_view>

namespace aeonian::log {

enum class Level { kDebug = 0, kInfo = 1, kWarn = 2, kError = 3 };

// Abre (trunca) o arquivo de log. Pode ser chamada uma vez.
void Init(const std::string& path, Level min_level);
void SetLevel(Level level);
Level GetLevel();
Level ParseLevel(std::string_view text, Level fallback);

void Write(Level level, std::string_view message);
void Writef(Level level, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

}  // namespace aeonian::log

#endif  // AEONIAN_HOST_LOG_H_
