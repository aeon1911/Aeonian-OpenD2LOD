# Ordem de Documentação dos Binários

Baseado na análise de dependências e na experiência da comunidade de engenharia reversa do Diablo II[reference:2][reference:3].

## Hierarquia de Dependências
Tier 0 (Fundação - Sem dependências de jogo)
└── Storm.dll [1175 funcs] - MPQ, memória, rede, criptografia
Tier 0.5 (Fundação - Construído sobre Storm)
└── Fog.dll [799 funcs] - Memória, debug, utilidades
Tier 1 (Serviços Centrais)
├── D2Lang.dll [314 funcs] - Localização, tabelas de string
└── D2CMP.dll [632 funcs] - DC6, DCC, DT1
Tier 2 (Fundação do Jogo)
└── D2Common.dll [2259 funcs] - Units, Items, Skills, Stats
Tier 3 (Subsistemas)
├── D2Sound.dll [375 funcs]
├── D2Win.dll [804 funcs]
├── D2Gfx.dll [388 funcs]
└── D2Net.dll [290 funcs]
Tier 3.5 (Backends de Render + Bibliotecas de Suporte)
├── D2DDraw.dll [329 funcs]
├── D2Direct3D.dll [~300 funcs]
└── D2Glide.dll [~250 funcs]
Tier 4 (Sistemas de Alto Nível)
├── D2Multi.dll [650 funcs]
├── D2Game.dll [5042 funcs]
└── D2Client.dll [5366 funcs]
Tier 5 (Pontos de Entrada)
├── D2Launch.dll [744 funcs]
└── Game.exe [384 funcs]


## Justificativa

- **Tier 0 e 0.5** são a base de tudo. Sem entender MPQ e memória, nenhum outro módulo pode ser implementado.
- **Tier 1** fornece localização e formatos gráficos, necessários para qualquer coisa visível.
- **Tier 2** é o coração dos modelos de dados do jogo. É a DLL mais crítica para a lógica.
- **Tier 3 e 3.5** são relativamente isolados e podem ser implementados em paralelo.
- **Tier 4** contém a lógica complexa e muitas vezes entrelaçada. Só deve ser atacado após os tiers anteriores estarem estáveis.
- **Tier 5** é a cola final.
