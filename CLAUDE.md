# CLAUDE.md

Projeto de RE/decompilação do Sub Rosa (2015).

**Language:** everything in the repository is in English (code, comments, docs, commit messages). Chat with the user is in Portuguese (BR). Old commits stay in Portuguese.

## Regras

- Nunca versionar nada de `original/` além de `README.md` e `MANIFEST.sha256`. Binários e assets são proprietários.
- `original/` é somente leitura. Ferramentas leem de lá e escrevem em `analysis/`, `build/` ou no scratchpad.
- Rodar Python com `-I` ao ler arquivos de `original/` ou `third_party/`.
- `config.txt` dos servidores tem senhas. Não copiar o conteúdo para docs ou commits.

## Binários-chave

- `original/client/subrosa.x64` — ELF64 Linux **com símbolos**. Fonte de verdade para nomes de funções.
- `original/client/subrosa.app/Contents/MacOS/subrosa` — Mach-O i386 com símbolos. Referência para layout 32 bits.
- `original/blackhouser/subrosadedicated024d.exe` — PE32 sem símbolos. Alvo do server manager (SHA-256 `11e00678…`).
- Endereços do 024d são sempre **RVAs** (relativos à base do módulo), como no server manager.

## Sessão na nuvem (Linux)

A nuvem faz o trabalho pesado do Ghidra. Compilar com MSVC e testar com o jogo fica no PC Windows.

1. Rode `bash tools/cloud/setup.sh`. Ele instala Java 21 e Ghidra 12.1.4 e baixa os binários oficiais do gart.sh. Leva de 5 a 10 minutos.
2. Rode `bash tools/ghidra/analyze_all.sh` para criar o projeto Ghidra. Leva de 10 a 30 minutos.
3. Faça commit só dos resultados versionáveis (`analysis/`, `docs/`, `src/`, `tools/`). Nunca de `original/`, `build/` ou `ghidra-project/`.

Se o download falhar, a sessão precisa de acesso de rede a `crypticsea.gart.sh` e `github.com`.

## Ferramentas

- `tools/re/strings.py <arquivo> [min]` — strings ASCII.
- `tools/re/elf_symbols.py <elf> [--sections]` — funções do ELF.
- `tools/re/survey_bins.py <pasta> [--manifest …] [--tsv]` — formato, símbolos e SHA-256 de todos os executáveis de uma pasta.
- `tools/re/manifest.py write|check` — hashes de `original/`.
- `tools/ghidra/analyze_all.sh [nome]` — importa e analisa binários. Caminhos em `tools/ghidra/env.sh`.
- `tools/ghidra/export_all.sh` — reexporta features sem reanalisar.
- `tools/ghidra/decompile.sh <programa> <saida.c> [rva ...]` — pseudo-C. Programa em pasta do projeto: `mac025/subrosa`.
- `tools/re/name_all.sh` — gera os CSVs de nomes de função.
- `tools/re/map_globals.py` — mapeia variáveis globais por votos.
- `tools/re/struct_fields.py` — levanta campos de structs do pseudo-C.
- `tools/formats/subrosa_formats.py` — leitores de `.srk`, `.sbl`, `.sbb`, `.sbc`, `.csm`.

## Plano

Ver `docs/plano.md`.
