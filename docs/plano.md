# Plano SubVulpes — decomp do Sub Rosa

## Contexto

O repositório já está inicializado (commit `b8ca658`): `original/` fora do git com manifesto SHA-256, server manager do amigo em `third_party/server-manager`, ferramentas em `tools/re/`, inventário em `docs/inventario.md`.

Objetivo: decompilar o Sub Rosa para melhorar servidores, adicionar features, otimizar e portar (Linux, macOS, navegador).

Decisões tomadas com você:

- **Estratégia:** decomp funcional + hooks (estilo re3/OpenRCT2). Cada função é reescrita em C e substitui a original dentro do executável real.
- **Começo:** servidor dedicado.
- **Protocolo:** compatível com os clientes originais 0.24.
- **Repo:** privado por enquanto.

Fatos técnicos que guiam o plano:

| Fato | Consequência |
| --- | --- |
| `subrosa.x64` (Linux) e o Mach-O i386 **têm símbolos**. Build de 2015-08-02, mesma data do `subrosa.exe` e do `subrosadedicated.exe` do cliente. | Nomes de ~817 funções de graça. Mach-O dá o layout de structs em 32 bits. |
| Todos os PE usam **MSVC 2010** (linker 10.0, `MSVCR100.dll`). | Hooks precisam respeitar as convenções do MSVC. Matching futuro é possível com VS2010. |
| O `subrosadedicated024d.exe` (2015-09-05) **importa `WSOCK32.dll`**. | Um `wsock32.dll` proxy na pasta do servidor carrega nosso código sem injetor. O cliente da galera já usa esse truque. |
| `.data` do dedicado tem ~636 MB (pools estáticos). | Precisa virar alocação dinâmica no servidor próprio e, sobretudo, no WASM. |
| O manager já mapeou RVAs, pools e offsets do 024d (`ANALISE-NPCS.md`, `ANALISE-BLACK-MARKET.md`, `native_sdk/*.h`). | Importar como rótulos iniciais. |
| Máquina: Python 3.14, Java 21, CMake, git. **Sem compilador C.** | Instalar VS 2022 Build Tools (x86) e Ghidra. |

---

## Fase 0 — Fundação de RE (primeira coisa a executar)

1. Instalar ferramentas: Ghidra 11.x (usa o Java 21 existente), VS 2022 Build Tools com MSVC x86. Documentar em `docs/setup.md`.
2. Criar projeto Ghidra local (gitignored) com: `subrosa.x64`, Mach-O i386, `subrosadedicated.exe` (ago), `subrosadedicated024d.exe` (set).
3. Script headless `tools/ghidra/import_symbols.py`: aplica os nomes do ELF/Mach-O e propaga para os PE via Version Tracking (ago → set são quase iguais).
4. **Confirmar a versão de protocolo:** comparar `serverrecv`/`clientrecv` e a constante de versão entre o cliente (ago) e o 024d. Responde se o cliente da galera fala com o 024d. Registrar em `docs/versoes.md`.
5. Analisar o `wsock32.dll` do cliente (mod de 2024): o que ele faz e se é seguro. Registrar em `docs/mods-terceiros.md`.
6. Documentar formatos de arquivo: `.sbc`, `.sbb`, `.sbl`, `.csm`, `.srk`, `config.txt`. Arquivo `docs/formatos.md` + parsers em `tools/formats/`.

**Saída:** `analysis/024d/functions.csv` (RVA, tamanho, nome, origem do nome, confiança) e `analysis/024d/globals.csv`. Esses CSVs são a fonte de verdade versionada. O projeto Ghidra não entra no git.

## Fase 1 — Structs e globais

1. Montar `src/include/sr_types.h` com as structs: `Player` (0x2408), `Human` (0x2B64), `Vehicle/Object` (0x8230), `Item`, `Connection` (0xF170), `Account`, `Event`. Usar o Mach-O i386 como referência de layout.
2. `static_assert` de tamanho e offset para cada campo conhecido.
3. Script que exporta as structs para o Ghidra (`tools/ghidra/apply_types.py`), para o decompilador mostrar campos com nome.

## Fase 2 — Infraestrutura de hooks

Layout novo em `src/`:

```
src/
  include/      sr_types.h, globals.h (macros GLOBAL(tipo, nome, rva))
  common/       matemática, vetores, parser, MT19937, bitstream (pbitwrite/pbitread)
  game/         humanos, itens, objetos, veículos, lógica, economia, cenários
  net/          pacotes, eventos, servercycle/serverrecv/serversend
  server/       dedicatedserverloop, contas, config
  platform/     sockets, timer, arquivos (abstração p/ Win/Linux/WASM)
hook/
  wsock32_proxy/  DLL proxy: repassa as 21 importações para o wsock32 do sistema
  hooks.c         tabela HOOK(rva, função), instalação com JMP de 5 bytes
```

Regras:

- `HOOK(0x50AE0, createhuman)` registra a substituição. Um `hooks.cfg` liga/desliga cada hook, para bisseção de bugs.
- Globais acessam a memória original via `GLOBAL(Player, players[256], 0xEDEDD00)` até o servidor ficar standalone.
- Spike obrigatório: verificar convenções de chamada do MSVC 2010 (`__cdecl`, `__fastcall`, registradores customizados de LTCG). Criar wrappers `naked` onde precisar.
- Primeiros hooks: funções folha (vetores, `pbitwrite*`) para validar o pipeline ponta a ponta.

## Fase 3 — Decomp do servidor, de baixo para cima

Ordem: utilitários → bitstream/pacotes → eventos (`createevent_*`) → jogadores/contas → itens/objetos/veículos → humanos (`humansimulation`, `bondsimulation2`) → lógica/economia/cenários → `servercycle` → `dedicatedserverloop`.

- `tools/progress.py` calcula % de funções e bytes substituídos a partir do CSV. Publicar em `docs/progresso.md`.
- Cada função nova só conta como feita depois de passar no teste diferencial (ver Verificação).

## Fase 4 — Servidor standalone (`subvulpes-server`)

1. Quando todo o código alcançável pelo dedicado estiver em C, compilar sem o `.exe` original.
2. Trocar os pools estáticos por alocação dinâmica e limites configuráveis.
3. Builds Windows x86/x64 e Linux x64. Atenção a structs com ponteiros ao passar para 64 bits.
4. Continuar 100% compatível com o cliente 0.24.

## Fase 5 — Servidor melhor

1. API de plugins nativa (C, carregada por DLL/.so) no lugar dos patches de memória.
2. Portar os módulos do manager como plugins: black market, comandos admin (`/heal`, `/tp`, `/npc`, `/imortal`...), mensagens de morte, caminhão mais rápido, dinheiro inicial, socos, bots.
3. Console de admin / RCON, logs estruturados, validações anti-cheat no servidor.
4. Lista de servidores própria se o master da Cryptic Sea (`crypticsea.com/anewzero/serverinfo.php`) não responder mais.
5. Perfilar `servercycle` e otimizar (tick rate, envio de pacotes, colisões).

## Fase 6 — Cliente

1. Mesmo método de hooks no `subrosa.exe` (que também importa `WSOCK32.dll`).
2. Reaproveitar `common/`, `game/`, `net/` do servidor. Faltam render, input, áudio, voz, menus, editores.
3. Cliente standalone com SDL2 + OpenAL. Trocar o OpenGL de pipeline fixo por GL 3.3 / GLES 3 (pré-requisito do WebGL2).

## Fase 7 — Portes

1. Linux, macOS (arm64), Windows x64 com o cliente standalone.
2. **Navegador (Emscripten/WASM):**
   - Render em WebGL2 (vem da Fase 6.3).
   - Rede: navegador não faz UDP. O servidor ganha transporte WebSocket (ou WebRTC DataChannel) ao lado do UDP.
   - Memória: pools dinâmicos da Fase 4.
   - Assets: o jogador carrega a própria cópia do jogo (seletor de arquivo + IndexedDB). Nada de assets hospedados.

---

## Arquivos críticos

- `original/blackhouser/subrosadedicated024d.exe` — alvo dos hooks do servidor.
- `original/client/subrosa.x64`, `.../MacOS/subrosa` — fontes de nomes e layouts.
- `analysis/symbols/subrosa_x64.functions.txt` — já gerado por `tools/re/elf_symbols.py`.
- `third_party/server-manager/ANALISE-*.md`, `native_sdk/*.h`, `core.py` — RVAs e offsets já mapeados. O `core.py` também mostra como validar o hash do 024d.
- `third_party/server-manager/tools/verify_*.py` — usam Unicorn para emular x86. Reaproveitar a ideia para testes diferenciais.

## Verificação

- **Integridade:** `python -I tools/re/manifest.py check` antes de cada sessão de RE.
- **Teste diferencial por função:** rodar a original (via Unicorn, como o manager faz, ou chamando a original dentro do processo) e a reescrita com as mesmas entradas e memória. Comparar retorno e memória escrita.
- **Teste em jogo:** servidor 024d + `wsock32.dll` proxy + cliente original. Checklist: conectar, andar, dirigir, atirar, comprar no telefone, morrer, round reiniciar. Rodar com todos os hooks ligados e depois só com os novos.
- **Rede:** gravar tráfego (Wireshark) do servidor original e do servidor com hooks no mesmo cenário. Comparar formato dos pacotes.
- **Progresso:** `tools/progress.py` em cada commit.

## Primeira entrega após aprovar

Fase 0 inteira: Ghidra + símbolos propagados para o 024d, `functions.csv`, confirmação de versão de protocolo, análise do `wsock32.dll` do cliente, e `docs/setup.md` com o que instalar.
