# Structs (Fase 1)

Headers gerados:

- [`src/include/sr_types.h`](../src/include/sr_types.h): `Player`, `Human`, `Item`, `Object`, `Connection`, `Account`, `vec3`, `Bone`, `InventorySlot`.
- [`src/include/sr_globals_024d.h`](../src/include/sr_globals_024d.h): RVAs das variáveis globais no 024d.

Não edite os headers à mão. O fluxo é:

1. Decompile o Mach-O i386 inteiro: `tools/ghidra/decompile.sh subrosa build/decomp/mac_i386_all.c`.
2. Levante os offsets acessados: `python -I tools/re/struct_fields.py build/decomp/mac_i386_all.c --out analysis/structs/mac --struct player=0x2408 ...`.
3. Dê nome aos campos em [`analysis/structs/names.csv`](../analysis/structs/names.csv). Cada nome precisa de uma evidência.
4. Gere os headers: `python -I tools/re/gen_types.py`.
5. Compile `src/tests/check_types.c` com MSVC x86. Os `static_assert` conferem cada tamanho e cada offset com nome.

## Por que o Mach-O i386

O Mach-O é 32 bits como o PE do 024d. Os tamanhos dos registros são iguais nos dois: `player` `0x2408`, `human` `0x2b64`, `object` `0x8230`, `item` `0x1ac`, `connection` `0xf170`. O ELF x64 tem os mesmos tamanhos para essas structs, porque elas não têm ponteiros.

## Validação no 024d

As funções `createplayer` (`0x28020`), `createhuman` (`0x50ae0`) e `createitem` (`0x1b070`) do 024d gravam as mesmas constantes nos mesmos offsets que o Mac:

| Struct | Offset | Valor gravado |
| --- | --- | --- |
| player | `+0x22f4..+0x2320` | 4 vetores de cor |
| player | `+0x23b4`, `+0x23b8`, `+0x23bc` | `rand() % 3`, `& 7`, `& 3` |
| human | `+0x24` | `0xe10` (3600) |
| human | `+0xc0` | `0x3c0` |
| item | `+0x10`, `+0x118..+0x120`, `+0x17c..+0x19c` | iguais ao Mac |

## Diferenças entre versões

| Campo | 0.24b (Mac) | 024d |
| --- | --- | --- |
| `human+0x34` na criação | `((rand() & 3) + 2) * 3600` | `((rand() & 3) + 3) * 3600` |

## Estado (critérios da Fase 1)

| Critério | Estado |
| --- | --- |
| 1. Structs do servidor no header, com tamanho certo | `Player`, `Human`, `Item`, `Object`, `Connection`, `ItemType`, `ObjectType`, `Bond`, `Event`, `Bullet`, `Body`, `Account`. Falta `Game` (struct única, tamanho de 32 bits ainda não confirmado). |
| 2. Sub-structs | `Bone` (posição, rotação, massa), `InventorySlot`, `vec3`. |
| 3. Cobertura de nomes ≥ 80% | **80,0%** dos acessos do servidor (`tools/re/struct_coverage.py`). |
| 4. Globais do 024d | Todos os arrays acima, mais `numofbonds`, `numofbullets`, `numofevents`, `ticks`. Confirmados em `analysis/024d/manual_globals.csv`. |
| 5. Validação | Compila no MSVC x86 com `/W4 /WX`. 148 `static_assert` de offset. Offsets conferidos no 024d. |

Capacidades (tamanho da global no ELF 0.24c ÷ tamanho do registro): player 256, human 512, item 1024, object 1024, connection 256, bond 16384, bullet 16384, event 65536 (buffer circular), particle 4096, objecttype 128, itemtype 64, body 32, account 65536.

## Achados de protocolo

- Pacote de entrada do jogador (`serverrecv`): nome (32 bytes) e `account_id` ao entrar. Depois, a cada quadro: 8 floats (`+0x7c..+0x98`), botões (`+0x9c`), `input_mode` (8 bits), `hud_mode` (4 bits), `num_actions` (8 bits).
- Eventos: o cliente confirma o último evento recebido (`connection.event_ack`, 16 bits). O servidor reenvia do `event_ack` até `numofevents`, no máximo 31 por pacote.
- Telefones dos times: número = `1111 × time` (`item.phone_id`).

## Próximos passos

- Nomear os campos de `Bone` (15 por corpo) com `human_buildmuscles` e `bondsimulation2`.
- Nomear `Object` (veículos) com `createobject` e `objectsimulation_car`.
- Achar o RVA de `connection` no 024d. O manager diz `0x15e39020`, mas o mapa de globais ainda não confirmou.
