# Análise de NPCs — Sub Rosa 024d

Análise estática do executável do servidor fornecido, SHA-256:

`11e006786d945d5731cb664fba718a094d8f02648b13fcc20c26c8614ea603fd`

Os nomes das rotinas abaixo são rótulos atribuídos pela análise do código de máquina; o executável não forneceu uma API pública com esses nomes. RVAs são deslocamentos relativos à base real do módulo, e não endereços absolutos.

## O que foi identificado

| RVA | Função observada | Evidência no código |
| --- | --- | --- |
| `0x28020` | Reservar/inicializar jogador | Procura slot com flag zero, limpa 0x2408 bytes, ativa o slot e inicializa campos. Retorna -1 quando não encontra slot. |
| `0x50AE0` | Criar corpo humano | Procura slot, limpa 0x2B64 bytes, liga ao jogador, inicializa vida em 100, ossos, posição e estado. Retorna -1 quando o pool está cheio. |
| `0x31800` | Criar veículo | Procura slot com flag zero, limpa 0x8230 bytes e preenche tipo, posição, matriz e parâmetros. Retorna -1 quando o pool está cheio. |
| `0x33350` | Criar veículo em uma rota | Calcula posição/orientação a partir da rede de trânsito e chama a rotina de criação de veículo. |
| `0x585B0` | Criar NPCs motoristas | Recebe uma quantidade; reserva jogador, atribui equipe 6 e flag de IA, cria veículo e corpo, liga o NPC ao carro e prepara rotas. |
| `0x1610` | Preparar/atualizar rota de IA | Lê nós, sentidos e conexões e preenche os campos de percurso do jogador. |
| `0x4F720` | Atualizar entradas da IA | É chamada no loop para jogadores ativos com flag de IA. Modifica entradas de movimento/ações. |
| `0x64A0` | Emitir informações de jogador | Preenche registros de eventos com flag de IA, equipe, nome e outros campos. |
| `0x51060` | Etapa da simulação de humanos | É chamada no loop principal depois de outras etapas de simulação. O nome é inferido do contexto. |

A rotina de NPCs é chamada originalmente com 24 em `0x5B204` e com 32 em `0x5BB00`. Isso explica por que mudar o byte em `0x5BAEA` só altera a quantidade de uma chamada de inicialização, sem oferecer um comando de spawn manual.

## Pools internos observados

| Pool | RVA inicial | Tamanho por registro | Capacidade inferida pelos limites dos loops |
| --- | --- | --- | --- |
| Jogador | `0xEDEDD00` | `0x2408` | 256 |
| Corpo | `0x15CBF80` | `0x2B64` | 512 |
| Veículo | `0x5592600` | `0x8230` | 1.024 |

Esses números são capacidades internas de estruturas, **não limites de jogadores humanos conectados**. Corpos mortos e outros veículos podem ocupar slots mesmo quando existem poucos jogadores.

## Implementação substituta da versão 0.3.0

A versão anterior usava a rotina de motoristas `0x585B0`. O usuário relatou que ela bugou a partida; aquela implementação foi retirada deste pacote. A causa completa no jogo não foi reproduzida aqui, portanto não atribuímos o problema a uma única instrução.

O módulo novo intercepta a chamada em `0x5CB79`, preservando seu destino original `0x33570`. Isso coloca a criação antes da preparação dos caches e da física do frame, em vez do ponto antigo `0x5CBFD`.

- Identifica uma conexão humana pelo pool de conexões em RVA `0x15E39020` (registro `0xF170`, player ID em `+0x10`, IP em `+0`, porta em `+4`) e contador em `0x1C4AFC0`.
- Fixa IP/porta/player ID da primeira conexão vista enquanto o módulo está ativo e valida a presença antes de cada criação. Não usa o primeiro slot do pool de jogadores, que pode pertencer a um bot.
- As conexões novas são acrescentadas ao fim em `0x34127`. A remoção em `0x337E3` copia a última conexão sobre a removida: depois de saídas anteriores à ativação, a ordem histórica já não pode ser inferida apenas do array. Para garantir o primeiro a entrar, ative numa sessão nova ou enquanto a primeira conexão original ainda existe no índice zero.
- Exige jogador ativo, flag de IA zero, corpo válido ligado a ele, vida positiva e ausência de veículo. Valida coordenadas/direção antes de reservar qualquer slot.
- Calcula posição com `x + sin(yaw)*distância`, `y + 0,15`, `z - cos(yaw)*distância`, usando posição global do corpo em `+0x48` e yaw do jogador em `+0x8C`. Distância inicial 3; pedidos adicionais avançam 1,5 por NPC.
- Chama `0x28020`, configura o novo slot (nome NPC, equipe 6), chama `0x50AE0(posição, matriz, player ID)` e valida corpo e vínculo sem carro.
- Liga o corpo ao novo jogador, inicializa ângulos, habilita IA de pedestre com lista de caminhos vazia, e emite informação de jogador por `0x64A0`. A criação do corpo emite seu próprio evento.
- Não chama criador de veículos, rotina de trânsito, missão ou inicialização de rota de motorista.
- Faz `FXSAVE`, limpa a pilha x87 temporária antes das chamadas e restaura com `FXRSTOR`. Salvar o estado sozinho não limpa valores x87 vivos, e as rotinas nativas usam vários níveis da pilha.

## Verificação ampliada

Além dos 22 testes de motor/protocolo e 102 comparações de relocações, o pacote contém `tools/verify_native_spawn.py` e `native_spawn_harness.c`. O harness foi executado com Unicorn x86, com o código original do executável para os alocadores, criação de corpo/articulações e atualização de IA.

32 condições são verificadas: vínculos, vida e ausência de carro, posição em duas direções, eventos nativos de corpo/jogador, múltiplos pedidos, rejeição por morte/carro/desconexão/pools cheios e preservação de seis valores x87 vivos ao redor de uma criação.

O teste usa estruturas de mundo sintéticas e stubs de dependências CRT; não é uma partida. O destino original do hook de rede e a verificação do security cookie são substituídos somente no harness. O módulo entregue mantém o destino original e as rotinas intactas.

## Limites ainda presentes

Ainda é necessário testar mapa carregado, física e replicação com clientes reais no Windows. Não há busca de chão ou espaço livre na posição proposta. A IA de pedestre inicia sem caminho e não possui comportamento novo de seguir/perseguir. Remover NPCs criados também não foi implementado. A versão continua experimental.
