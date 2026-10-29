# Avisos Legais e Isenção de Responsabilidade

## Propriedade intelectual da Blizzard

Diablo II, Diablo II: Lord of Destruction e todos os seus assets associados
(gráficos, áudio, dados, logotipos) são marcas registradas e propriedade
intelectual da Blizzard Entertainment, Inc.

O projeto Aeonian-OpenD2LOD **não é afiliado, endossado ou patrocinado** pela
Blizzard Entertainment.

## Como o projeto lida com os arquivos do jogo

Este repositório contém **apenas código-fonte original** do projeto, specs e
dados de interoperabilidade (endereços/assinaturas curtas). Nós **não
distribuímos** o `Game.exe`, as DLLs, os MPQs nem nenhum arquivo de dados do
jogo (`.txt`, `.dc6`, `.dcc`, `.ds1`, `.wav`, ...).

O Aeonian **não substitui o jogo**: ele só funciona quando o usuário possui
uma cópia legítima e legalmente adquirida do **Diablo II: Lord of Destruction
1.14d** e instala o Aeonian nessa cópia. O instalador **não altera o
`Game.exe`** e preserva a `ijl11.dll` original para restaurá-la na
desinstalação.

Este documento descreve o modelo técnico do projeto e **não é aconselhamento
jurídico**; não afirma o que a Blizzard permite ou proíbe. Cada
usuário/contribuidor é responsável por respeitar os termos do jogo e a lei da
sua jurisdição.

## Código-fonte (GPLv3)

Todo o código do projeto é licenciado sob a GNU General Public License v3.0
(ver `LICENSE`). Modificações e redistribuições devem manter a licença.

## Dependências de terceiros

- **Nenhuma** nas DLLs que rodam no jogo (`aeonian.dll`, `ijl11.dll` proxy,
  plugins de exemplo): só a biblioteca padrão e a Win32.
- **StormLib** (MIT, Ladislav Zezula) — usada **apenas** pelo extrator de MPQ
  opcional (`tools/mpq_extractor`, `-DAEONIAN_BUILD_TOOLS=ON`), baixada pelo
  CMake em tempo de build; não é redistribuída no repositório.
- **D2GL** (GPL-3.0, bayaraa — https://github.com/bayaraa/d2gl) — wrapper
  Glide/DDraw → OpenGL **opcional**, que o Aeonian integra para alto
  desempenho em monitores modernos. **Não é redistribuído neste repositório**;
  o instalador o copia de uma pasta fornecida pelo usuário (`--d2gl-dir`). Quem
  redistribuir o D2GL deve cumprir a GPL-3.0 (o código-fonte está no repositório
  do autor). Versão testada: v1.3.3.
- **Intel JPEG Library (`ijl11.dll`)** — é a DLL original do jogo; o proxy do
  Aeonian apenas encaminha as chamadas a ela. Não é redistribuída.
