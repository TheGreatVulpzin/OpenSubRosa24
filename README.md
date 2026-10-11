# SubVulpes

Projeto de análise e decompilação do **Sub Rosa** (Cryptic Sea, build de 2015, servidor 0.24d).

Objetivos:

1. Entender o código do jogo (cliente e servidor dedicado).
2. Recriar o código em C, compilável e equivalente ao original.
3. Melhorar os servidores: desempenho, estabilidade, ferramentas de admin, mods.
4. Adicionar features novas.
5. Portar para outras plataformas (Linux, macOS moderno, ARM) e para o navegador (WebAssembly).

## Estrutura

| Pasta | Conteúdo |
| --- | --- |
| `original/` | Arquivos originais do jogo. **Fora do git.** Veja [original/README.md](original/README.md). |
| `analysis/` | Dados extraídos dos binários (símbolos, tabelas, notas de RE). |
| `docs/` | Documentação. Comece por [docs/inventario.md](docs/inventario.md). |
| `tools/re/` | Scripts de RE sem dependências (strings, símbolos ELF, manifesto). |
| `src/` | Código decompilado / reimplementado (ainda vazio). |
| `third_party/server-manager/` | Server Manager 0.7.1 (patches de memória para o dedicado 024d). |

## Primeiros passos

```bash
python -I tools/re/manifest.py check
```

```bash
python -I tools/re/elf_symbols.py original/client/subrosa.x64
```

## Aviso legal

Sub Rosa é propriedade da Cryptic Sea. Este repositório não distribui binários nem assets originais.
É preciso ter uma cópia do jogo para usar qualquer coisa daqui.
