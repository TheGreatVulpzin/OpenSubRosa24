# Inventário inicial

Levantamento feito em 2026-10-11, só com análise estática (sem executar nada).
Hashes completos em [`original/MANIFEST.sha256`](../original/MANIFEST.sha256).

## Binários

| Arquivo | Formato | Símbolos | SHA-256 (início) | Observação |
| --- | --- | --- | --- | --- |
| `client/subrosa.x64` | ELF64 x86-64, Linux, build 2015-08-02 | **Sim (não stripped)** | `9afeea22` | Melhor alvo para nomear funções. 2757 funções, ~817 do jogo. |
| `client/subrosa.app/.../MacOS/subrosa` | Mach-O **i386** | Sim (`_gameloop`, `_createhuman`...) | `78119827` | 32 bits como o Windows: ajuda a casar layout de structs x86. |
| `client/subrosa.exe` | PE32 i386, GUI | Não | `294db16c` | Cliente Windows. Steam (`steam_api.dll`). |
| `client/subrosadedicated.exe` | PE32 i386, console | Não | `54f18a19` | Dedicado que veio com o cliente. |
| `blackhouser/subrosadedicated024d.exe` | PE32 i386, console | Não | `11e00678` | Dedicado **0.24d** (2015-09-04). É o alvo do server manager. |
| `client/wsock32.dll` | PE32 DLL proxy (2024) | — | `99d3fada` | Mod de terceiros carregado pelo cliente (strings: `conveniencestore1`, `texture/cash_thousand.png`). Precisa de análise. |

> Hipótese a confirmar: o cliente é da mesma versão (0.24) do dedicado 024d.
> Comparar funções equivalentes (ex.: `serverrecv`) entre o ELF e o PE.

## O que os símbolos do ELF revelam

- Código em **C procedural** (só 20 símbolos C++ `_Z`). Nomes em minúsculo, sem prefixo: `gameloop`, `humansimulation`, `serverrecv`.
- Bibliotecas embutidas: GLee (OpenGL), stb_image, zlib, Mersenne Twister (`genrand_*`), Speex. Externas: SDL2, OpenAL, Steam.
- `.bss` tem **~731 MB**: pools estáticos gigantes (jogadores, corpos, veículos, itens). Problema direto para o porte web (WASM).
- Lista completa: [`analysis/symbols/subrosa_x64.functions.txt`](../analysis/symbols/subrosa_x64.functions.txt).

### Subsistemas (por prefixo de função)

| Subsistema | Exemplos |
| --- | --- |
| Loop / entrada | `main`, `gameloop`, `dedicatedserverloop`, `getinputs_*`, `checkkeyboard` |
| Rede | `serverrecv`, `serversend`, `servercycle`, `clientrecv`, `clientsend`, `pbitwrite*/pbitread*`, `packobjectpacket`, `createevent_*` |
| Master server | `getmasterserverip`, `getonlineserverlist` (HTTP para `www.crypticsea.com/anewzero/serverinfo.php`) |
| Humanos / ragdoll | `createhuman`, `humansimulation` (21 KB), `human_calculatearmangles`, `bondsimulation2` (30 KB) |
| Veículos | `objectsimulation_car`, `objectsimulation_heli`, `objecttrainsimulation`, `getenginetorque*` |
| Itens / armas | `createitem`, `itemsimulation`, `itemweaponsimulation`, `bulletsimulation` |
| Lógica de jogo | `logicsimulation_round/_world/_driving`, `logic_cardealer_buy`, `logic_gunstore_buy`, `economysimulation` |
| Cenários / missões | `createscenario_*`, `createmission_acquisition/transaction/threeway` |
| Mapa / cidade | `area_*`, `building_*`, `city_*`, `landscape_*`, editores `leveleditor_*`, `buildingedit_*` |
| Render | `render*`, `drawhud`, `drawmenuitems`, `generatetexture_*` (texturas procedurais) |
| Voz | `encodespeex`, `decodespeex`, `calculateplayervoice` |
| Contas | `createaccount`, `loadaccountsserver`, `accountdeathtax` |

## Dados do jogo

| Extensão | Qtd | Provável conteúdo |
| --- | --- | --- |
| `.sbl` | 60 | Blocos (`data/block`) |
| `.sbb` | 58 | Prédios (`data/building/%s.sbb`) |
| `.sbc` | 2 | Cidade (`data/city.sbc`, 1 MB) |
| `.csm` | 2 | Modelos (`loadmodelcsm`) |
| `.png` / `.wav` | 52 / 26 | Texturas e sons |
| `.glvs` / `.glfs` | 10 / 10 | Shaders GLSL (vertex/fragment) |
| `.srk` | 2 | Contas/estado do servidor (`server.srk`) |

## Servidor Black Houser

- `subrosadedicated024d.exe` + `config.txt` (gametype 4, 24 jogadores, round 30 min).
- `server.srk` com contas salvas.

## Server Manager 0.7.1 (`third_party/server-manager`)

Feito pelo amigo, com ChatGPT. Python + Tkinter, só Windows.

- Abre o processo do dedicado 024d e aplica **patches de memória** (`WriteProcessMemory`), ou injeta DLLs nativas.
- Só funciona com o hash exato `11e00678…`. Endereços são RVAs fixos.
- Plugins em JSON (`plugins/`), convertidos de tabelas Cheat Engine (`source_tables/*.CT`).
- DLLs nativas em C (`native_sdk/`): black market, comandos de admin (`/heal`, `/tp`, `/npc`, `/imortal`...), mensagens de morte.
- **Valor para o decomp:** `ANALISE-NPCS.md` e `ANALISE-BLACK-MARKET.md` já mapeiam RVAs, pools e offsets de structs do 024d:

| Pool (024d) | Tamanho do registro | Capacidade |
| --- | --- | --- |
| Jogador | `0x2408` | 256 |
| Corpo humano | `0x2B64` | 512 |
| Veículo | `0x8230` | 1024 |
| Conexão | `0xF170` | — |

Essas informações devem ser importadas como rótulos no projeto de RE (ver plano).
