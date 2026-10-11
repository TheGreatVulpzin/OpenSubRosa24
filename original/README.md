# original/

Arquivos originais. **Não são versionados** (são proprietários da Cryptic Sea).
Só este README e o `MANIFEST.sha256` entram no git.

| Pasta | Conteúdo |
| --- | --- |
| `client/` | Sub Rosa (cópia "SubRosa da galera"): cliente Windows, Linux x64, macOS i386 e dedicado. |
| `blackhouser/` | Servidor "Black Houser": `subrosadedicated024d.exe` + config e mapa. |
| `archives/` | Os `.rar`/`.zip` originais, intocados. |

Para verificar se os arquivos batem com o manifesto:

```bash
python -I tools/re/manifest.py check
```

Quem clonar o repositório precisa colocar os arquivos nessas pastas manualmente.
