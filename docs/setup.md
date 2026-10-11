# Setup do ambiente

Ambiente de referência: Windows 10, Git Bash e PowerShell.

## Ferramentas

| Ferramenta | Versão | Onde | Para que serve |
| --- | --- | --- | --- |
| Python | 3.14 | `C:\Python314` | Scripts em `tools/`. Sem dependências externas. |
| Java (Temurin) | 21.0.7 | `C:\Program Files\Eclipse Adoptium\jdk-21.0.7.6-hotspot` | Requisito do Ghidra. |
| Ghidra | 12.1.4 | `E:\tools\ghidra_12.1.4_PUBLIC` | Decompilador. SHA-256 do zip: `ddac49f9…d2d4db`. |
| VS 2026 Build Tools | MSVC 14.50 | `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` | Compilador C para x86 (DLL de hooks). |
| Windows SDK | 10.0.26100.0 | `C:\Program Files (x86)\Windows Kits\10` | Headers e libs do Windows. |
| CMake | — | `C:\Program Files\CMake` | Build do código em `src/` e `hook/`. |

O disco C: tem pouco espaço livre. Por isso o Ghidra fica no E:.

## Arquivos originais

1. Copie os arquivos do jogo para `original/` (veja [original/README.md](../original/README.md)).
2. Verifique os hashes:

```bash
python -I tools/re/manifest.py check
```

## Ghidra: análise e exportação

O script importa os binários no projeto local `ghidra-project/` (fora do git). Ele exporta as características das funções para `build/features/*.jsonl`. A primeira execução leva de 10 a 30 minutos.

```bash
JAVA_HOME="/c/Program Files/Eclipse Adoptium/jdk-21.0.7.6-hotspot" tools/ghidra/analyze_all.sh
```

Para abrir o projeto na interface gráfica, execute `E:\tools\ghidra_12.1.4_PUBLIC\ghidraRun.bat` e abra `ghidra-project/subvulpes.gpr`.

## Nomes de funções

Este comando leva os nomes do ELF Linux e do Mach-O para o servidor 024d:

```bash
python -I tools/re/match_functions.py --target build/features/ded024d.jsonl --ref build/features/linux_x64.jsonl --ref build/features/mac_i386.jsonl --out analysis/024d/functions.csv
```

## Compilar para x86 com o MSVC

Use o ambiente x86 do MSVC no `cmd`:

```bat
"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars32.bat"
```

O aviso `'vswhere.exe' não é reconhecido` aparece, mas não impede a compilação.
