# Formatos de arquivo (Sub Rosa 0.24)

Leitores em [`tools/formats/subrosa_formats.py`](../tools/formats/subrosa_formats.py).

Fonte: decompilação do cliente Linux 0.24b (`subrosa.x64`, com símbolos). Funções: `saveaccountsserver`, `loadaccountsserver`, `saveaccountsclient`, `loadblock`, `building_save`, `building_load`, `leveleditor_save`, `leveleditor_load`, `savemodelcsm`, `loadmodelcsm`, `createaccount`.

Regras gerais:

- Little-endian. Inteiros de 32 bits, salvo indicação.
- O jogo lê pelo ponteiro `freadptr`. No Mac PowerPC ele aponta para `freadswap` (troca a ordem dos bytes). No x86 é `fread` direto.
- O primeiro inteiro de cada arquivo é a versão. O jogo recusa versões fora da faixa.

Teste: todos os arquivos do 0.24b passam e são lidos até o último byte (29 `.sbb`, 30 `.sbl`, `city.sbc`, `ak47.csm`, 2 `server.srk`). O gravador de `server.srk` gera arquivos idênticos aos originais.

```bash
python -I tools/formats/subrosa_formats.py original/client/data/city.sbc
```

## `user/server.srk`: contas do servidor

| Campo | Tipo | Descrição |
| --- | --- | --- |
| versão | int | 1 a 3. O jogo grava 3. |
| key | uint | Global `account`. Número aleatório (`genrand_int32`) gerado quando não existe arquivo. |
| n | int | Número de contas (máximo `0xffff`). |
| contas | n × 60 bytes | Ver abaixo. |

Conta no arquivo (60 bytes). Em memória o registro tem `0x4c` bytes. Os campos `+0x28` e `+0x40..+0x4b` não vão para o arquivo.

| Offset na memória | Tipo | Campo | Descrição |
| --- | --- | --- | --- |
| `+0x00` | uint | id[0] | Copiado de `player+0x24`. Identifica o jogador. |
| `+0x04` | uint | id[1] | Copiado de `player+0x28`. |
| `+0x08` | char[32] | nome | O jogo grava os 32 bytes, com lixo depois do `\0`. Nome repetido ganha `(n)`, com corte em 24 caracteres. |
| `+0x2c` | int | dinheiro | Conta nova: 1000. Pode ficar negativo (dívida). |
| `+0x30` | int | desconhecido | Não lido na versão 1. |
| `+0x34` | int | aparência A | Criação: `rand() % 3`. |
| `+0x38` | int | aparência B | Criação: `rand() & 7`. |
| `+0x3c` | int | aparência C | Criação: `rand() & 3`. |

Versão 1 não tem os 4 últimos campos. Na versão 2, se A, B e C são 0, o jogo sorteia de novo.

## `client.srk`: contas no cliente

| Campo | Tipo |
| --- | --- |
| versão | int (1) |
| n | int |
| registros | n × 4 int |

Provável: cada registro guarda a chave de um servidor e o `id` do jogador nele. Falta confirmar.

## `data/block/*.sbl`: bloco (peça de prédio)

| Campo | Tipo | Descrição |
| --- | --- | --- |
| versão | int | 1 ou 2. |
| cabeçalho | 10 int | `blocktemp+0x40..+0x64`. O 10º é o número de superfícies. |
| superfícies | ver abaixo | |
| n_caixas | int | Só versão 2. |
| caixas | n × (8 vec3 + 6 int + 1 int) | Só versão 2. Registro de `0x7c` bytes. 8 cantos, 6 valores por face, 1 flag. Provável: colisão. |

Superfície (registro de `0x15c` bytes em memória):

1. 6 int, lidos na ordem `+8, +0, +0xc, +4, +0x150, +0x154`.
2. 16 pontos (grade 4×4). Cada ponto: posição (3 float) + UV (2 float). É um patch Bézier bicúbico (`addbeziersurface`).

## `data/building/*.sbb`: prédio

| Campo | Tipo | Descrição |
| --- | --- | --- |
| versão | int | 6 a 10. O jogo grava 10. |
| nome | char[64] | |
| size_x | int | Colunas. Padrão 24. |
| size_z | int | Linhas. Padrão 24. |
| andares | int | Padrão 4. |
| n_texturas | int | |
| texturas | n × char[64] | Ex.: `concrete`, `greycarpet`, `ceiling0`. |
| n_blocos | int | |
| blocos | n × char[64] | Nomes de `.sbl`. Ex.: `astairwell`, `rooftop`. |
| células | ver abaixo | Grade com borda: andar `0..andares-1`, linha `0..size_z`, coluna `0..size_x`. |
| extras | 16 × 6 int | Só versão ≥ 9. |

Em memória a grade é `[64 andares][64 linhas][64 colunas]` de `0x48` bytes, em `building+0x4c`.

Célula no arquivo (ordem de leitura):

| Offset | Tipo | Observação |
| --- | --- | --- |
| `+0x00`, `+0x04` | int, int | Célula ocupada quando um deles não é 0. Usam bits altos como flags. |
| `+0x10` | int | Só versão ≥ 7. |
| `+0x08`, `+0x0c`, `+0x14` | int ×3 | Provável: índices de textura (piso, teto). |
| `+0x18..+0x36` | 16 × int16 | Provável: peças/texturas por lado. |
| `+0x38`, `+0x3c` | int ×2 | `+0x3c` zerado em versão < 8. |
| `+0x40`, `+0x44` | int ×2 | Só versão ≥ 10. |

## `data/city.sbc`: cidade

Arquivo único, ~1 MB. O jogo só aceita a versão 6. Dados vão para a global `level_editor`.

| Campo | Tipo | Descrição |
| --- | --- | --- |
| versão | int | 6. |
| n_cruzamentos | int | |
| cruzamentos | n × 3 int | `(x, y, z)` em unidades de tile. Ex.: `(160, 0, 288)`. |
| n_ruas | int | |
| ruas | n × (5 int + char[32]) | Os 2 primeiros int são os cruzamentos das pontas. Nome: `Main`, `6th Street`... |
| tiles | 1024 × 1024 bytes | Mapa de tiles. Valores vistos: 0, 16, 17, 18 e múltiplos de 16 até 176. |
| n_prédios | int | |
| prédios | n × (char[64] + 4 int) | Nome do `.sbb` + `(x, y, z, rotação 0-3)`. Ex.: `gunshop` em `(147, 6, 261, 2)`. |

O `city.sbc` do 0.24b tem 54 cruzamentos, 84 ruas e 39 prédios.

## `*.csm`: modelo 3D

| Campo | Tipo | Descrição |
| --- | --- | --- |
| assinatura | 4 bytes | `CSMo` |
| versão | int | 1 |
| n_vértices | int | |
| desconhecido | int | `model+4`. Valor 8 no `ak47.csm`. |
| n_triângulos | int | |
| posições | n_vértices × vec3 | |
| vetor B | n_vértices × vec3 | No `ak47` começa com (1, 0, 0). Provável: tangente. |
| vetor C | n_vértices × vec3 | No `ak47` começa com (0, 1, 0). Provável: bitangente. |
| índices | n_triângulos × 3 int | Vértices de cada triângulo. |
| normais | n_triângulos × vec3 | Normal da face (vetores unitários). |
| índices C | n_triângulos × 3 int | Provável: índices de UV. |

## Pendências

- Nomear os campos de célula do `.sbb` com o código de `building_setupbuild`.
- Nomear os 10 inteiros do cabeçalho do `.sbl` e os 5 inteiros de rua do `.sbc`.
- Confirmar `client.srk` com o código de `loadaccountsclient`.
- Descobrir o campo `+0x30` da conta.
