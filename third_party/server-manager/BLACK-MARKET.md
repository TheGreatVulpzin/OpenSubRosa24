# Black Market — versão 0.6.3

Implementação própria baseada na análise da DLL enviada e do dedicado 024d. A DLL antiga não é executada, incorporada nem necessária. O ZIP contém `plugins/native/sr_combined.dll`, `plugins/native_combined.json`, os fontes C/DEF/assembly em `native_sdk` e o build em `tools/build_market_dll.py`.

## Mapeamento

| Recurso | RVA / estrutura do dedicado 024d |
| --- | --- |
| Hook das compras | callsite 0x5CC0C, original chama 0x4C8C0 |
| Hook do ataque | callsite 0x5CBFD, original chama 0x51060 |
| Equipar item | 0x1B870, cdecl(itemID, parentItem=-1, humanID, slot) |
| Criar item | 0x1B070, cdecl(type, posição, velocidade, rotação) |
| Remover item | 0x1B3C0, cdecl(id) |
| Evento de item | 0x6380 |
| Mensagem por telefone | 0x626D, entrada após o gate da função 0x6260 |
| Interseção de personagem | 0x2F130 |
| Pool de itens | 0x91022C0, 1024 registros de 0x1AC |
| Pool de personagens | 0x15CBF80, 512 registros de 0x2B64 |
| Pool de jogadores | 0xEDEDD00, 256 registros de 0x2408 |
| Fase / tick | 0x90DC560 / 0x90DC578 |

As compras ocorrem depois do processamento dos personagens/dígitos, antes da rotina original de itens/telefones. O ataque ocorre em callback separado antes da simulação de personagens, para que o impulso do golpe e o dano estejam presentes ao entrar na rotina original. A trampoline guarda flags, registradores e o estado x87/SSE com FXSAVE/FXRSTOR, inicializa uma pilha x87 própria e então executa a rotina original. Nenhuma função de jogo é chamada de Configure, Stop, DllMain ou de uma thread remota.

## Vida dos objetos

São criados dois telefones do tipo 23, estado 3. O módulo registra seus IDs e confere tipo, atividade, estado e coordenadas de ancoragem antes de reutilizar/remover um slot. Se o slot deixar de corresponder, a referência é descartada. O módulo tenta recriar telefones faltantes durante a fase ativa, com intervalo de 60 ticks quando o pool está cheio. Uma redução do contador da simulação reinicia os temporizadores de compra.

O telefone só aceita compras do personagem associado a um jogador real ativo, vivo, com ID/corpo válido e próximo do ponto. O número é consumido antes das verificações; uma compra recusada exige discar novamente. Somente após CreateItem retornar um ID válido são descontados o dinheiro e iniciado o intervalo. Preços podem ser zero. Itens comprados ficam no mundo depois da desativação.

PrepareStop sinaliza a parada e aguarda até 3000 chamadas Sleep(1) pela limpeza executada no próximo tick. Retorno 6 indica que a simulação não confirmou a limpeza: retome o servidor e repita Desativar. Depois da confirmação, o gerenciador restaura o callsite e chama Stop. A DLL/contextos permanecem residentes até fechar o processo. Para trocar a DLL compilada, reinicie o servidor.

## Diferenças da DLL enviada

- Preços, coordenadas, intervalos, dano e alcance ajustáveis; ativação durante uma partida já iniciada.
- Intervalo por jogador, compartilhado entre os dois pontos, em vez do intervalo por telefone.
- Ataque de faca simplificado, com dano fixo (25 por padrão), alcance 1,5 e intervalo 20 ticks. Usa tipo 24 / estado 1 e a interseção nativa de personagens. Não inclui os multiplicadores por osso da antiga DLL. A 0.6.0 envia o evento nativo de impacto corporal. O impulso de golpe foi acrescentado na 0.5.2. Não acrescenta novos modelos ao cliente; o tipo 24 é reaproveitado como na implementação original.
- Não inclui comandos de administração, alterações de carros ou outros extras da antiga DLL.
- Mercado, chat e comandos ficam no único módulo Black Market + Chat + Comandos (DLL).

## Validação e limites

50 testes Python verificam o gerenciador e protocolos. 110 verificações x86 em duas bases executam o DLL compilado, sua trampoline (incluindo preservação de dois valores vivos na pilha x87) e as rotinas reais de criação/remoção de itens do executável fornecido. As APIs Windows, eventos de rede/mensagens, detach do item segurado são simulados. Os testes básicos anteriores ainda usam colisões simuladas; o teste de regressão novo executa a rotina real de interseção de personagens, com ossos em posições sintéticas e _CIsqrt substituído por FSQRT. Verifica alvo à frente, atrás, ao lado, fora do alcance, faca rotacionada, orientação não unitária, dano zero, ausência de clique, IDs inválidos, propriedade do item, intervalo, morte, proteção de spawn e invulnerabilidade. A aparência/interação do cliente não foi validada neste ambiente. O carregador real do Windows também não foi executado neste ambiente.

Para teste no jogo: ative primeiro só o mercado; confira os dois telefones, a mensagem de preços, compras 24/12, o saldo e o uso dos itens. Desative para verificar a remoção dos telefones e ative novamente. Depois teste junto do chat e demais módulos. O spawn de NPC foi preservado exatamente como a versão confirmada.

## Correção do ataque em 0.5.1

A trajetória agora usa `item + 0x9C/0xA0/0xA4`, a terceira linha da rotação da faca, com sentido negativo como no trecho 0x10002FB1–0x10003016 da DLL fornecida. O segmento inicia 0,11 unidade atrás da origem e termina na direção da ponta, respeitando o alcance configurado. A normalização tolera pequenas variações do comprimento da base física. A orientação da cabeça em `human + 0x2D0` não participa mais do teste. A 0.5.2 adiciona o impulso do golpe no braço da mão ativa; os multiplicadores por osso continuam fora deste ataque simplificado. A 0.6.0 acrescenta o evento de impacto original.

O caso de regressão colocou a faca com eixo Z positivo e o alvo uma unidade à frente (Z negativo). A DLL 0.5.0 falhou nesse caso, enquanto a 0.5.1 diminuiu a vida de 100 para 75 usando a colisão real do dedicado. As compras e o spawn de NPC mantiveram seus testes anteriores.

Atualização exige encerrar o processo do servidor para substituir a DLL residente.

## Mão ativa, timing e diagnóstico em 0.5.2

A rotina nativa 0x44BDxx seleciona o slot de mão pelo bit 0x10 de `human + 0xBC`. O primeiro slot começa em 0x291C (quantidade / primeiro ID em 0x2920), o segundo em 0x2940 (primeiro ID em 0x2944). Cada slot ocupa 0x24 bytes. O golpe verifica atividade, tipo 24, estado 1, dono em item+0x18 e slot em item+0x20. O comando primário de ataque é o bit 1 na mão ativa. O botão secundário não foi convertido em outro comando de dano.

O callback SR_AttackTick executa antes da função humana nativa. O callback SR_Tick segue responsável pelas compras e pela limpeza. A parada desativa os dois callbacks e o gerenciador restaura os dois callsites depois da limpeza dos telefones.

A opção `knife_debug` fica no offset 68 do contexto, agora com 72 bytes. Diagnóstico é desligado por padrão e escreve `sr_black_market_debug.log` na pasta de trabalho do servidor, com no máximo uma linha por segundo por personagem. `damage-applied` indica redução da vida; `no-body-intersection` indica ausência de contato no teste; `target-protected` indica proteção; `not-active-knife` expõe o tipo/estado/dono/slot que foram lidos; `no-active-hand-item` indica slot vazio/inválido; `use-left-click` registra clique secundário. Falha de abertura do arquivo não interrompe a simulação.

O teste novo usa a função real EquipItem para colocar a faca na mão 1, confirma a ligação nativa do item e verifica dano antes da atualização humana. Repete na mão 0, testa mão inativa, clique secundário e bolso, e verifica as linhas do diagnóstico com APIs de arquivo simuladas.
