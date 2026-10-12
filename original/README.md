# original/

Arquivos originais. **Não são versionados** (são proprietários da Cryptic Sea).
Só este README e o `MANIFEST.sha256` entram no git.

| Pasta | Conteúdo |
| --- | --- |
| `client/` | Sub Rosa (cópia "SubRosa da galera"): cliente Windows, Linux x64, macOS i386 e dedicado. |
| `blackhouser/` | Servidor "Black Houser": `subrosadedicated024d.exe` + config e mapa. |
| `archives/` | Os `.rar`/`.zip` originais, intocados. |
| `vanilla/` | Versões oficiais baixadas para comparação (0.22, 0.24b, 0.24c, 0.24d, 0.25b). Os pacotes ficam em `vanilla/archives/`. |

Fontes do `vanilla/`: `https://crypticsea.gart.sh/Sub Rosa/` e `https://archive.org/details/sub-rosa-0.22`.

O pacote com todas as versões (`SUB ROSA ALL.rar`, 2,1 GB, de `https://archive.org/download/sub-rosa-all`) é processado na nuvem. Ver `docs/versoes-todas.md`.

Para verificar se os arquivos batem com o manifesto:

```bash
python -I tools/re/manifest.py check
```

Quem clonar o repositório precisa colocar os arquivos nessas pastas manualmente.
