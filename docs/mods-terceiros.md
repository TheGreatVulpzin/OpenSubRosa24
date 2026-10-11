# Mods de terceiros no cliente

O cliente da galera tem otimizações fanmade. Elas estão em DLLs que não são da Cryptic Sea.

## `client/wsock32.dll`

| Campo | Valor |
| --- | --- |
| SHA-256 | `99d3fadae0e3b24f0cb24c664fd5e84f5710bfa94a5e4d61a6d785fc483646dd` |
| Build | 2023-03-14, MSVC 2022 (linker 14.34), C++ |
| Tipo | DLL proxy. Exporta as 75 funções do `wsock32.dll` do Windows. |
| Decompilação | `build/decomp/wsock32_fan.c` (gerado, fora do git) |

### Como carrega

1. O `subrosa.exe` importa `WSOCK32.dll`. O Windows carrega primeiro a cópia da pasta do jogo.
2. No `DllMain`, a DLL carrega o `wsock32.dll` real de `GetSystemDirectoryW` e guarda os 75 endereços para repassar as chamadas.
3. A DLL instala um hook em `subrosa.exe+0x1ab0c` (início do jogo). Quando o jogo passa por ali, a DLL aplica todos os patches.
4. A DLL também cria um gerador MT19937 com semente de `std::random_device`.

### Patches no `subrosa.exe`

Os nomes vêm de `analysis/client_win/functions.csv` (casador automático). "?" indica função sem nome.

| RVA | Função | Patch | Efeito provável |
| --- | --- | --- | --- |
| `0x9c3d2` | `createlevel` | JMP para a DLL (9 bytes) | Muda a criação do mapa. |
| `0xda051` | `?` (`0xd9fa0`) | JMP para a DLL | Função grande do mapa/render (usa `2052` e `24680`). |
| `0xe3c94` | `rendersprites` | JMP para a DLL | Muda o desenho de sprites. |
| `0x106324` | `setupperspectiveviewport` | JMP para a DLL | Muda a câmera (FOV ou proporção da tela). |
| `0x7cdaf` | `gameloop` | 6 bytes de NOP | Remove uma instrução do loop principal. |
| `0x7a859` | `resetgame` | 1 byte = 0 | Muda uma constante ao reiniciar a partida. |
| `0xfafe0` | `resettimer` | RET | Desliga a função. |
| `0xfb020` | `starttimer` | RET | Desliga a função. |
| `0xfb040` | `?` (provável `stoptimer`) | RET | Desliga a função. |
| `0xd9fc9`, `0xd9fd0` | `?` (`0xd9fa0`) | Troca 7 bytes cada | Muda instruções da mesma função do mapa/render. |
| `0xda334`, `0xda3fb` | `?` (`0xd9fa0`) | `cmp [ebp-0x80], 0x20` e `0x10` | Muda dois limites de loop. |
| `0xe8456` a `0xe8482`, `0xe8760` a `0xe8774` | `generatetexture` | 9 trocas de 7 bytes | Muda a geração de texturas procedurais. |

Os três `RET` desligam os timers de medição de tempo (profiling). Isso provavelmente é parte da "otimização".

### Funções interceptadas (hook completo)

| RVA | Função | Handler na DLL | O que faz |
| --- | --- | --- | --- |
| `0x79ad0` | `resetgame` | `FUN_10002db0` | Se o modo de jogo é 0, usa um modo próprio (5). Remove jogadores e corpos, edita blocos do mapa, cria 10 veículos. Parece um cenário customizado. |
| `0xac190` | `setupmodel` | `FUN_10004760` | Adiciona modelos (provável: `conveniencestore1`). |
| `0x10ac40` | `setuptexture` | `FUN_10003370` | Adiciona texturas (`texture/cash_thousand.png`). |

### Funções do jogo que a DLL chama

`createplayer` (`0xd4750`), `deleteplayer` (`0xd4a20`), `createhuman` (`0x850b0`), `createitem` (`0x96cf0`), `createobject` (`0xc5780`), `linkitem` (`0x99d50`), `scenario_armhuman` (`0xddaf0`), `area_marksector` (`0x39230`), `loadtexturesetalpha` (`0x10a2f0`), `generatetexture` (`0xe7ff0`).

Funções sem nome que a DLL chama, com a identificação provável:

| RVA | Evidência | Provável |
| --- | --- | --- |
| `0xc6ff0` | Usa `0x2B64` (corpo) e `0x8230` (objeto). A DLL chama para os 512 slots. | `deletehuman` |
| `0x43c60` | Recebe coordenadas de bloco. Usa `2052` e `24680`. | `area_setblock` |

### Riscos para o SubVulpes

| Risco | Avaliação |
| --- | --- |
| Servidor | Nenhum. A DLL só existe no cliente. |
| Protocolo de rede | Nenhum patch toca nas funções de rede. O cliente com mod já joga no 024d original. |
| Conflito com nossos hooks (Fase 6) | Real. Não colocar hooks nossos nos endereços acima enquanto a DLL estiver ativa. |
| Nome do arquivo (Fase 6) | Nosso DLL também se chama `wsock32.dll`. Renomear o mod para `wsock32_fan.dll` e carregar ele a partir do nosso. |

Indícios de segurança: a DLL só importa funções de memória, carga de DLL e CRT. Não importa funções de internet, processos, registro ou escrita de arquivos. Conclusão: baixo risco.

## `client/SDL2.dll`

Arquivo de 2024-02-21. O jogo original é de 2015. Alguém trocou o SDL2 por uma versão mais nova.

## Pendências

- Ler os handlers `FUN_10004760` e `FUN_10003370` e os desvios para `0x10002c3b`, `0x10002c8b`, `0x10002cab` e `0x10002d0b`.
- Descobrir quando o modo de jogo é 0 no cliente (menu, offline ou partida).
- Descobrir a versão do `SDL2.dll`.
