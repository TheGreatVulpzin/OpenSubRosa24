# Todas as versões (pacote SUB ROSA ALL)

## Fonte

- Pacote: `SUB ROSA ALL.rar`, de `https://archive.org/download/sub-rosa-all`.
- Tamanho: 2.222.098.574 bytes. SHA-256 `9198f70edbb35e3e44f8918a4307dc1f018904731242a33e6efcc12a7bda2fdf`.
- Montado por "Jenerick". Tem 99 pacotes, da 0.01 até a 0.38f.
- Processado na nuvem em 2026-10-12. Os arquivos ficaram fora do repositório (`~/sr-all/`).

A tabela completa (303 executáveis, sem duplicatas) está em `analysis/versoes/executaveis.tsv`.
Para gerar de novo: `python -I tools/re/survey_bins.py <pasta extraída> --manifest original/MANIFEST.sha256 --tsv`.

## Conclusão principal

- **Não existe 0.24d para Linux ou Mac no pacote.** A série 0.24 tem só os mesmos arquivos que já temos em `original/vanilla/`.
- O 0.24d só existe como `subrosadedicated024d.exe` (PE32, sem símbolos). O hash é o mesmo do Black Houser.
- O binário com símbolos mais perto do 024d **depois** dele é o cliente Mac i386 da 0.25 (outubro de 2015, 4573 símbolos).
- Antes do 024d, os mais perto continuam os servidores Linux 0.24b/0.24c (461 funções) e o cliente 0.24b.

## Versões e símbolos

"Build" é a data do cabeçalho PE (TimeDateStamp). ELF e Mach-O não têm essa data.

| Versão | Build PE | Windows (PE) | Linux (ELF) | Mac (Mach-O) |
| --- | --- | --- | --- | --- |
| 0.01 – 0.05 | 2012-06 | `subrosa.exe` (VC6), sem símbolos | – | – |
| 0.06, 0.06b | 2012-08 | `subrosa.exe` (VC6) | `subrosa` x64 **2433 funções**; `subrosadedicated32/64` **296 funções** | – |
| 0.07, 0.07b | 2012-09/10 | `subrosa.exe` (VC6) | `subrosadedicated32/64` **321 funções** | `subrosa` i386 **4350 símbolos** |
| 0.15 test | 2014-02-26 | `subrosa.exe`; dedicado não oficial (marek9911) | – | – |
| 0.21 | 2014-08-14 | `subrosa.exe`; dedicado não oficial (marek9911) | – | i386 **4502 símbolos** |
| 0.22 | 2014-10-31 | `subrosa.exe`; dedicado não oficial (mordekai) | – | i386 **4503 símbolos** |
| 0.23 | 2015-01-01 | `subrosa.exe`; dedicado não oficial (marek9911) | `subrosa64` **2681 funções** | i386 **4539 símbolos** |
| 0.24b | 2015-08-02/04 | `subrosa.exe`, `subrosadedicated.exe`, `subrosadedicated024b.exe` | `subrosa.x64` **2757**; `subrosadedicated024b` **461** | i386 **4662 símbolos** |
| 0.24c | 2015-08-04 | `subrosadedicated024c.exe` | `subrosadedicated024c` **461 funções** | – |
| 0.24d | 2015-09-05 | `subrosadedicated024d.exe` | – | – |
| 0.25, 0.25b | 2015-10-06 | `subrosa.exe`, `subrosadedicated025b.exe` | `subrosa.x64` e `subrosadedicated025b` sem símbolos | i386 **4573 símbolos** |
| 0.25 patches | 2015-10-16/18 | `subrosatest_b` … `_g.exe` | – | – |
| 0.25-1 | 2015-10-06 | `subrosa.exe` (igual ao da 0.25) | `subrosa.x64` sem símbolos | x86-64 **4570 símbolos** |
| 0.25c, 0.25d | 2017-03/05 | `subrosa.exe` | `subrosa.x64` sem símbolos | x86-64 **4570 símbolos** (igual ao da 0.25-1) |
| 0.26a | 2016-08-31 | `subrosa.exe` (Traffic Test) | – | – |
| 0.27a | 2017-08-02 | `subrosa.exe` | – | – |
| 0.28a, 0.28b | 2017-09 | `subrosa.exe` | – | – |
| 0.29a | 2017-12/2018-01 | `subrosa.exe` | `subrosa.x64`, `subrosadedicated028.x64` sem símbolos | x86-64 1903 símbolos |
| 0.30a Test 9 | 2018-08-08 | `subrosa.exe` | – | – |
| 0.31 – 0.35 | 2018-09 a 2019-06 | cliente e dedicado | cliente e dedicado sem símbolos | x86-64 1942 a 2044 símbolos |
| 0.36 betas (v, x, y, z) | 2019-09/10 | cliente e dedicado | sem símbolos | x86-64 2044 símbolos |
| 0.36a, 0.36b | 2019-10 | cliente e dedicado | sem símbolos | – (sem Mac) |
| 0.37 | 2020-06 a 2020-08 | cliente e dedicado | sem símbolos | – |
| 0.38 | 2021-03 a 2021-09 | PE32+ x64, MSVC 14.26 | sem símbolos | – |

Notas:

- Todos os PE de 0.15 a 0.37 são i386 com linker 10.0 (MSVC 2010). A 0.38 muda para x64 e MSVC 2019.
- Nenhum PE tem tabela COFF nem caminho de PDB.
- Desde a 0.25, o Linux sai **stripped**. Só o Mac continua com símbolos.
- Da 0.29 em diante, o Mac tem menos símbolos (~1900–2044). Mas ainda tem nomes de funções do jogo (`_gameloop`).
- Os dedicados 0.15, 0.21, 0.22 e 0.23 são **não oficiais** (feitos pela comunidade).

## Arquivos que já tínhamos

Comparação com `original/MANIFEST.sha256`. Hashes iguais:

| Arquivo no pacote | Já temos em |
| --- | --- |
| 0.22 `subrosa.exe` e Mac `subrosa` | `vanilla/0.22/` |
| 0.24b cliente (`subrosa.exe`, `subrosa.x64`, Mac, `subrosadedicated.exe`) | `client/` e `vanilla/0.24b/` |
| 0.24b e 0.24c dedicados (Linux e Windows) | `vanilla/0.24b_*`, `vanilla/0.24c_*` |
| 0.24d `subrosadedicated024d.exe` | `blackhouser/` e `vanilla/0.24d_Dedicated_Windows/` |
| 0.25b `subrosadedicated025b` (Linux) | `vanilla/0.25b_Dedicated_Linux/` |
| 0.25/0.25b cliente (`subrosa.exe`, `subrosa.x64`, Mac i386) | `vanilla/0.25b/` (baixado nesta sessão) |

O `subrosa.x64` da 0.25 é o mesmo até a 0.25d. Todo o resto é novo para o projeto.

## Binário novo em `original/`

| Arquivo | Origem | SHA-256 |
| --- | --- | --- |
| `vanilla/0.25b/Sub Rosa 0.25b/subrosa.app/Contents/MacOS/subrosa` | `0.25/b/Sub Rosa 0.25b.zip` do gart.sh. É o mesmo binário da 0.25 do pacote. | `771dc2ab…c099` |

O `tools/cloud/setup.sh` agora baixa esse pacote (`vanilla/0.25b/`).
No projeto Ghidra ele fica na pasta `/mac025`. Motivo: o nome `subrosa` já é do Mac 0.24b.
Nome curto das features: `mac025_i386`.

## Efeito nos nomes do 024d

O Mac 0.25 entrou como `--ref` no passo do 024d em `tools/re/name_all.sh`.
As duas execuções partiram do mesmo `functions.csv` (o do commit anterior).

| Execução | Funções do 024d com nome |
| --- | --- |
| Sem o Mac 0.25 | 189 de 340 |
| Com o Mac 0.25 | 194 de 340 |
| Com o Mac 0.25 + `playerai` manual | **195 de 340** |

Mudanças:

- Nomes novos: `city_addintersection`, `serpnoise2d`, `addbeziersurfacetexture`, `lineintersectarea`, `createevent_sound`, `playersimulation` (0xa2a0).
- `objecttype_addparticle` mudou de 0xb0b0 para 0x28a90 (18 → 96 votos; tamanho mais perto do ELF).
- 0xb540 mudou de `buildcitystreet` para `setuptimers`. O tamanho (381 bytes, sem chamadas) bate com `setuptimers` (436 no ELF). `buildcitystreet` tem 4457 bytes.
- `lineintersectlevel` mudou de 0x21b40 para 0x4e500. Confiança baixa (0.333). Conferir.
- O Mac 0.25 trocou 0x4f720 de `playerai` para `playersimulation`. **Errado**: a função chama `ai_assessthreats` e `ai_playercar`. Corrigido em `analysis/024d/manual_names.csv`.

Cuidado: no Mac 0.25 as funções são bem menores que no 0.24b (ex.: `playerai` tem 4091 bytes contra 6497). O código mudou entre as versões. Nomes que vêm só dele merecem conferência.

## Outras coisas no pacote

- `Essential mods for versions before 0.25/`: um `SDL2.dll` para quem usa teclado de idioma latino (para digitar `/` e `?` no chat, versões antes da 0.26). Também um `.txt` com um link de Discord para o patch do novo master server.
- `readme.txt`: lista de servidores Discord da comunidade.
