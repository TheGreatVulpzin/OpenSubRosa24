# CLAUDE.md

Projeto de RE/decompilação do Sub Rosa (2015). Idioma do projeto: português (BR).

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

## Ferramentas

- `tools/re/strings.py <arquivo> [min]` — strings ASCII.
- `tools/re/elf_symbols.py <elf> [--sections]` — funções do ELF.
- `tools/re/manifest.py write|check` — hashes de `original/`.

## Plano

Ver `docs/plano.md`.
