# Comandos da DLL unificada — 0.7.1

Ative **Black Market + Chat + Comandos (DLL)**. Use o chat do jogo para `/admin <senha admin do servidor>`, depois `/bmhelp`. A DLL usa a permissão admin nativa do jogador (`+0x2C`); não altera a senha nem concede admin. `Comandos admin = 0` desativa esta interpretação de comandos e encaminha o texto ao servidor normalmente.

| Comando | Resultado |
| --- | --- |
| `/imortal` | Liga/desliga vida fixa em 100 em quem executa. Mantém impactos e efeitos do dano, exige admin e personagem vivo. |
| `/heal` | Vida 100, limpa indicador de morte e embriaguez. |
| `/kill` | Mata o próprio personagem. |
| `/item N` | Cria item 0–26 e equipa numa mão livre. A faca 24 recebe estado de ataque 1. Falha sem cobrar quando não há mão ou item livre. |
| `/vehicle N` | Cria veículo 0–13 a quatro unidades à frente da cabeça, com cor 0. Não coloca o jogador dentro dele. |
| `/arm N` | Uniforme 1–6, variação de roupa 4 e tenta colocar duas bandagens no slot 6. |
| `/heavydude` | Multiplica os 15 valores físicos por 100000, como no código original. Evita reaplicar em valores já grandes. |
| `/delh` | Apaga o personagem mais próximo interceptado na mira, até 100 unidades; não apaga quem está executando. |
| `/team N` | Muda a equipe 0–5 e envia atualização nativa. |
| `/deactivate ID` | Alterna o estado ativo do jogador de ID 0–255. Para reativar, outro admin usa o mesmo comando. Não seleciona por nome. |
| `/give admin ID` | Um administrador concede admin ao jogador conectado de ID 0–255. O destinatário pode usar comandos admin imediatamente, sem `/admin` e sem senha. |
| `/npc` | Cria um NPC humano sem carro, a 3 metros à frente de quem executa. Exige admin, personagem vivo e fora do veículo. O NPC usa IA sem rota e pode ficar parado. |
| `/bmhelp` | Mostra ajuda dos comandos. |

## Comandos extras de comandinhos.CT

Também ficam na mesma opção **Black Market + Chat + Comandos (DLL)** e usam a mesma permissão admin. Não instale a tabela do Cheat Engine ao mesmo tempo. `Comandos admin = 0` desativa a interpretação de todos estes comandos também.

| Comando | Efeito |
| --- | --- |
| `/turn N` | Copia nome, olhos, pele e cabelo da conta salva de índice N para seu jogador. Não transfere dinheiro, identificadores de conta, equipe ou permissão. |
| `/herobrine` | Muda apenas os olhos para o tipo 3 usado pelo script para o efeito Herobrine. |
| `/tp` | Teleporta você para a superfície atingida pela sua mira, com folga vertical de 1 unidade. Busca até 2000 unidades; falha se não atingir o mapa. Exige personagem vivo, a pé. |
| `/gobase N` | Teleporta você para as coordenadas da base N da tabela abaixo. Exige personagem vivo, a pé. Não troca sua equipe. |
| `/sound N` | Envia o som de índice N na superfície atingida pela mira, com volume 1 e pitch 0,15 (grave/lento), como na tabela original. Exige personagem vivo e superfície na mira. |

### Índices

`/turn` usa a **posição da conta na base salva do servidor**, começando em zero; não é o ID de conexão do `/deactivate`. O índice depende da ordem das contas salvas; não há consulta dessa lista pela interface nesta versão. A aparência é aplicada ao jogador atual; o comando não faz login na conta copiada nem altera seus identificadores. Não acrescenta uma função de edição permanente de aparência ao SRK.

| `/gobase` | Base da tabela | Coordenadas X, Y, Z |
| --- | --- | --- |
| 0 | Goldmen Inc | 178, 25, 453 |
| 1 | Monsota | 1084, 25, 418 |
| 2 | OXS International | 1117, 25, 955 |
| 3 | Nexaco | 349, 25, 955 |
| 4 | Pentacom | 674, 25, 1347 |

Essas coordenadas são as fixadas no CT para o mapa original. Não há busca de chão nem verificação de obstáculos para `/gobase`; mapas modificados podem exigir coordenadas diferentes.

`/sound` aceita **0–47**, correspondendo à quantidade de sons identificada na inicialização do cliente enviado. O ID **21** corresponde a `bullethitbody01.wav` nesse cliente. Por exemplo, `/sound 21` envia esse efeito no ponto da mira, com pitch 0,15. A reprodução depende do cliente e dos arquivos de áudio presentes; não é um comando de texto para escolher músicas ou tocar um arquivo externo.

**`/nick` não está implementado na tabela:** o CT declara a string, mas não tem ramo que execute uma troca de apelido. Por isso não foi criado um comportamento novo para esse nome.

### Ajustes em relação ao CT

A tabela original usa busca por substring sem limites de índices e lê 36 bytes para copiar um campo de nome de 32 bytes. A conversão exige comando no começo da mensagem, verifica os argumentos e copia no máximo 31 bytes com terminador NUL. Para teleportar, o CT colapsa os 15 ossos numa única posição; esta implementação aplica o mesmo deslocamento a cada osso, preserva a forma do personagem, atualiza as posições anteriores e zera a velocidade. Também desloca equipamento diretamente vinculado ao personagem.

Os comandos de aparência enviam atualização nativa de jogador e de corpo quando há personagem válido. As posições de `/tp` e `/sound` são obtidas pela rotina de interseção do mapa no RVA 0x31630. O som é enviado pela rotina nativa 0x16DB0.

Cada comando responde no chat e no console. ID ou sintaxe inválida retorna instrução de uso. Comandos de personagem exigem um corpo ativo vinculado ao jogador. Os limites, seleção da mão livre, seleção do alvo mais próximo e falhas sem índices inválidos são ajustes em relação à DLL antiga, que aceitava valores sem verificação.

A DLL não é uma cópia binária da original: reimplementa as funções identificadas e usa o gerenciador para configuração e hooks reversíveis. **Carregue pelo gerenciador**; injetar esta DLL isoladamente pelo Cheat Engine não fornece o contexto de configuração. Não injete a DLL antiga junto, pois ela instala hooks nas mesmas rotinas.

O componente de chat registra nome/mensagem, incluindo tentativas de comando; `Chat: mostrar ID = 1` acrescenta o ID. Textos limitados a 63 bytes e caracteres de controle são saneados no console.

Os testes executam o binário compilado em emulação, com permissões, parâmetros, passagem de mensagens comuns e todos os comandos. Item/equipamento e evento de impacto foram exercitados com código real do servidor. Chamadas de criação de veículo, troca de roupa, exclusão de personagem e evento de equipe foram simuladas; confirme seus efeitos na partida. Som depende da reprodução pelo cliente. Na 0.6.2 foram executadas 232 verificações nativas do conjunto em duas bases. Novos testes cobrem todos os cinco comandos, índices inválidos, permissão, cópia sem transferência de conta/saldo, esqueleto sem colapso, ausência de alvo, personagem no carro e o evento real de som. Interseção de mapa e chamadas de atualização de aparência foram simuladas; teste esses efeitos no jogo.

## `/npc` na versão 0.7.1

O comando cria um NPC por pedido, sem carro e com o nome NPC. Usa sua posição e direção, com distância de 3 unidades e altura adicional de 0,15. Exige admin e personagem vivo a pé. O pedido é processado no próximo ponto seguro do loop; repetir enquanto houver pedido pendente retorna uma mensagem para aguardar. O NPC possui IA de pedestre sem rota e pode permanecer parado. Ao desativar o módulo, o pedido pendente é cancelado; NPCs já criados permanecem. Use uma área aberta e plana: não há busca de chão nem teste de obstáculos na posição de destino.

A criação usa as rotinas nativas 0x28020 e 0x50AE0. O novo hook fica em 0x5CB7E, antes da atualização de caches e simulação, e substitui o script separado de spawn, removido da lista. A versão passou em 74 testes do gerenciador e 426 verificações nativas dos comandos em duas bases, incluindo criação real de corpo, orientação, slot de IA, pedidos repetidos, morte/desconexão, argumentos inválidos, falta de permissão e pools cheios. Os testes usam um mundo sintético em emulação; ainda é necessária a confirmação em partida no Windows.

## Conceder administrador

Exemplo: `/give admin 3` concede admin ao jogador conectado no slot 3. Somente quem já tem admin pode executar o comando. Rejeita IDs fora de 0–255, slots vazios/inativos e NPCs. Os dois jogadores recebem uma confirmação. Se o alvo já tem admin, o comando apenas informa isso.

A permissão usa o mesmo campo nativo do `/admin` e vale enquanto o jogador permanece conectado. Não altera a senha nem grava uma lista permanente de administradores. Após desconectar e reconectar, será necessário conceder novamente ou autenticar com `/admin`. O comando não exige que o administrador ou destinatário esteja vivo. `Comandos admin = 0` desativa sua interpretação, como os demais comandos da DLL.

A versão 0.7.1 passou em 74 testes do gerenciador e 426 verificações nativas dos comandos em duas bases. Os testes de concessão cobrem permissão do emissor, sintaxe, IDs inválidos, slots inativos, NPCs, jogador já administrador e uso de comando pelo destinatário após a concessão. Falta validar a concessão na partida no Windows.

## `/imortal`

Digite `/imortal` para ligar e novamente para desligar. O comando atua em você; não aceita ID ou outros argumentos. Exige admin e personagem vivo em uma partida ativa. Mantém a vida em 100 sem ligar a flag de god mode, preservando impactos, empurrões e sinais do dano. Termina ao trocar de corpo, desconectar, reiniciar a partida ou desligar a DLL/comandos. A nova DLL precisa de reinício do servidor antes da atualização.

Na 0.7.1, as mensagens privadas dos comandos usam o ID do corpo atual, resolvido no momento da resposta. Após o respawn, elas aparecem no novo personagem. Sem corpo vivo, são registradas somente no console. Os testes de morte têm mais 374 verificações nativas em duas bases. Os testes emulados não substituem uma partida no Windows.
