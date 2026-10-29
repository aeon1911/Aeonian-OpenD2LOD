# ae_mpq_extract — extrator de arquivos MPQ (Mo'PaQ) para uso local.
#
# Usa StormLib (MIT, autor: Ladislav Zezula) baixado via FetchContent com tag
# fixa para reprodutibilidade. As dependencias (zlib/bzip2/tommath/tomcrypt)
# vem bundled, entao nao ha dependencia de sistema.
#
# IMPORTANTE (cleanroom): este extrator e apenas uma ferramenta. Os arquivos
# extraidos NAO devem entrar no repositorio — extrair para um diretorio fora
# do projeto (ex.: junto a instalacao do jogo).

# O StormLib (FetchContent) e declarado na raiz do projeto; aqui so linkamos
# o alvo `storm` ja disponivel.
add_executable(ae_mpq_extract main.cpp)
target_link_libraries(ae_mpq_extract PRIVATE storm ae_warnings)
