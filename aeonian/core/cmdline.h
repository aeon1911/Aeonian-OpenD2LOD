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

// aeonian/core/cmdline.h
//
// Montagem da linha de comando entregue ao Game.exe: acrescenta flags dos
// mods (por exemplo `-direct`) sem duplicar as que o usuario ja passou.

#ifndef AEONIAN_CORE_CMDLINE_H_
#define AEONIAN_CORE_CMDLINE_H_

#include <string>
#include <string_view>
#include <vector>

namespace aeonian {

// Devolve `original` seguido de cada flag de `extra` que ainda nao aparece
// como token, garantindo a AUSENCIA dos tokens de `remove` (sem diferenciar
// maiusculas), mesmo os vindos de `extra`. O resto de `original` e preservado
// byte a byte (inclusive o caminho entre aspas).
std::string BuildCommandLine(std::string_view original,
                             const std::vector<std::string>& extra,
                             const std::vector<std::string>& remove = {});

}  // namespace aeonian

#endif  // AEONIAN_CORE_CMDLINE_H_
