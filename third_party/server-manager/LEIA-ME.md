# Sub Rosa 24 — Gerenciador de scripts 0.7.1

A 0.7.1 corrige o crash das mensagens de morte de NPCs/jogadores e as respostas dos comandos após o respawn do admin. **Reinicie o servidor antes de atualizar os arquivos e ativar os scripts.**

A 0.6.9 corrigiu e libera **Ejetar do carro**, para colisões que acionam o dano do veículo.

A 0.6.8 adicionou organização por arrastar e soltar, com pastas que podem ser abertas e fechadas.

A 0.6.7 adicionou **Caminhão mais rápido**, um módulo independente com ajustes de faixa de velocidade e força do motor, inicialmente 2x. Ative o módulo na partida para modificar todos os caminhões, incluindo os que já estão no mapa.

A 0.6.6 habilitou **Caminhão na concessionária** como módulo separado, executado no loop do servidor. Mantém preço configurável e verificação de limites do catálogo. Reinicie o servidor antes de atualizar o pacote.

A 0.6.2 adiciona os comandos de `comandinhos.CT` à opção **Black Market + Chat + Comandos (DLL)**: `/turn`, `/herobrine`, `/tp`, `/gobase` e `/sound`. Use `/admin <senha admin do servidor>` e `/bmhelp` no chat. Veja [COMANDOS-DLL.md](COMANDOS-DLL.md) para exemplos e índices.

A 0.6.5 adicionou `/give admin ID`: um admin concede a permissão ao jogador conectado indicado, sem ele precisar digitar a senha. Use `/admin <senha>` e depois `/npc` para criar um NPC a 3 metros à sua frente, estando vivo e fora do veículo. Reinicie o servidor antes de atualizar a DLL. O editor SRK continua removido.

**Atualização:** feche o gerenciador anterior. Extraia em uma pasta nova e copie somente `settings.json` para manter configurações. Os módulos separados de mercado/chat continuam fora do pacote.

## Usar a DLL unificada

Conecte ao dedicado 024d, selecione **Black Market + Chat + Comandos (DLL)**, ajuste os parâmetros e clique em **Ativar**. O mercado vende faca pelo número 24 e bandagem pelo 12 nos dois telefones. O chat aparece no console. Para os comandos, dentro do jogo use `/admin <senha admin do servidor>` e `/bmhelp`.

Comandos: `/heal`, `/kill`, `/item`, `/vehicle`, `/arm`, `/heavydude`, `/delh`, `/team`, `/deactivate` e os cinco comandos extras. Veja [COMANDOS-DLL.md](COMANDOS-DLL.md). Parâmetros podem ser editados após desativar e aplicados ao reativar. O servidor precisa estar rodando os ticks para limpar os telefones ao desativar.

## Requisitos e execução

Windows, Python 3 com Tkinter e o executável `subrosadedicated024d.exe` já analisado. Abra `Iniciar.bat`. Não é preciso Cheat Engine nem compilador para usar as DLLs incluídas. Use **Atualizar servidores**, escolha o PID e clique em **Conectar**. A lista permite arrastar scripts e pastas com o mouse, salvando a organização automaticamente. Scripts não são ativados automaticamente ao abrir.

## Organizar scripts com o mouse

- Segure o botão esquerdo sobre o nome de um script ou pasta e arraste. A linha destacada e a mensagem abaixo da lista indicam onde será colocado ao soltar.
- Solte perto da borda superior ou inferior de uma linha para colocar antes ou depois dela. Solte no centro de uma pasta para colocar dentro dela, mesmo se estiver fechada.
- Para tirar um script de uma pasta, solte junto de uma linha fora dela ou no espaço vazio no fim da lista.
- Clique em **Nova pasta** para criar um grupo. Se uma pasta estiver selecionada, o novo grupo será criado dentro dela. **Renomear** altera a pasta selecionada.
- Use a seta à esquerda do nome para abrir ou fechar uma pasta. A organização e o estado aberto/fechado ficam em `settings.json` e voltam ao abrir o programa.
- **Excluir pasta** remove somente o grupo e coloca seus scripts e subpastas no nível acima, na mesma posição. Não apaga arquivos nem desativa os scripts.
- **Subir/Descer** continuam disponíveis e movem dentro do mesmo nível. Pressione Esc durante um arraste para cancelar.

As pastas são grupos visuais; não é necessário mover os arquivos JSON ou DLL. Ao copiar seu `settings.json` da versão anterior, a ordem antiga é mantida. Scripts adicionados depois entram no fim da lista.

## Módulos incluídos

| Módulo | Configuração na interface | Observação |
| --- | --- | --- |
| Caminhão mais rápido | Faixa de velocidade e força de 1x a 4x | Padrão 2x nos dois ajustes. Afeta caminhões existentes e novos. |
| Ejetar do carro | Sem parâmetro | Ejeção nas colisões que acionam dano; modelo 11 isento. |
| Dinheiro inicial | Valor em dinheiro | Padrão 2.500. Restaura o valor original 1.000 ao desativar. Não modifica todos os saldos atuais. |
| Sistema de socos | Dano por velocidade, bônus na cabeça, empurrão, intervalo e limiar | Preserva a fórmula da tabela original. O limiar é comparado depois de multiplicar a velocidade pelo dano; o intervalo usa unidades internas do script. |
| Área segura no spawn | Limites positivos X/Z | Preserva a lógica de tiros e atropelamentos da tabela. Não foi ampliada para tratar socos. |
| Estado crítico | Sem parâmetro | Modifica o comportamento com pouca vida. |
| Correção de proteção de spawn | Limiar de velocidade ao quadrado | A tabela calcula a soma dos quadrados, sem tirar a raiz. |
| Clunky Ragdolls | Sem parâmetro | Troca a condição no ponto original, preservando o tamanho de 2 bytes da instrução. |
| Bots agressivos | Sem parâmetro | Modifica os pontos da tabela original. |
| Spawn na concessionária | Três coordenadas | Mantém a ordem de coordenadas usada na tabela. |
| Mensagens + morte instantânea | Sem parâmetro | Modo combinado. |
| Somente morte instantânea | Sem parâmetro | Alternativa ao modo combinado. |
| Somente mensagens de morte | Sem parâmetro | Alternativa ao modo combinado. |
| Máximo de jogadores | 1 a 32 | Não expulsa automaticamente quem já entrou. |
| Quantidade de NPCs | 0 a 128 | Modifica a constante de criação. Não cria/remove imediatamente os NPCs existentes. |

As três opções de morte compartilham os mesmos pontos de código. O gerenciador rejeita a ativação simultânea. Os três modos usam a DLL `sr_death_messages.dll`, com validação dos IDs, nomes copiados com limite e mensagens de até 63 caracteres. Há uma mensagem por morte. A morte instantânea adianta o temporizador e deixa o jogo aplicar as penalidades e separar o corpo uma única vez. Corpos antigos não alteram o vínculo do jogador com seu novo personagem.

**Caminhão na concessionária está disponível na 0.6.7.** Ative o módulo com a partida aberta. Ele inclui um caminhão no catálogo de cada concessionária, por $12000 (configurável), e repõe a oferta quando ela desaparece. Com oito ofertas, substitui apenas a última. Desativar para a reposição e restaura os dois valores globais alterados; ofertas existentes e veículos comprados não são apagados. A compra usa a rotina normal do servidor. O caminhão aparece na lista de compra, não estacionado automaticamente no mapa.

**Ejetar do carro está disponível na 0.6.9.** Ative para ejetar os ocupantes quando uma colisão aciona o dano do veículo. Não é um comando manual e não reage a toda encostada leve. O módulo verifica os 512 slots de corpo e valida o jogador antes de alterar seu estado. O modelo 11 fica isento, conforme a intenção da tabela original. Mantém o tipo do veículo e a rotina de dano do jogo. Desativar interrompe novas ejeções; quem já saiu não volta automaticamente ao banco.

## Usar `/imortal`

Ative **Black Market + Chat + Comandos (DLL)** e mantenha **Comandos admin** em 1. No chat, use `/admin <senha>` (ou receba admin por `/give admin ID`) e digite `/imortal`. A vida do seu personagem passa a ser mantida em **100**. Digite `/imortal` novamente para desligar. O comando afeta somente quem o executa e exige personagem vivo em uma partida ativa.

Os impactos, empurrões e efeitos do dano continuam; o módulo não ativa a flag de god mode. A vida é restaurada antes das verificações nativas de morte e nas atualizações da simulação. A ativação vale para o corpo atual e termina ao trocar de personagem, desconectar, reiniciar a partida, desligar os comandos ou desativar a DLL. Efeitos já sofridos, como sangramento, não são apagados ao desligar.

## Ajustar o desempenho do caminhão

Selecione **Caminhão mais rápido**, mantenha os dois multiplicadores em **2** e clique em **Ativar**. A faixa de velocidade amplia a rotação útil do motor; a força ajuda a acelerar e carregar peso. Ambos aceitam valores de 1 a 4. Para mudar, desative, ajuste e ative novamente. A velocidade final depende da física e da carga; 2x não garante o dobro da velocidade na pista. Desativar restaura o comportamento original do motor.

## Adicionar mais scripts com o servidor aberto

Os módulos do gerenciador são arquivos **JSON** na pasta `plugins`.

- Use **Adicionar script JSON** para escolher um módulo, ou coloque o arquivo na pasta e clique em **Recarregar pasta plugins**.
- Pode adicionar novos IDs sem desativar os recursos que já estão ativos.
- Para editar/remover o arquivo de um módulo ativo, desative esse módulo primeiro. Os demais podem continuar ligados.
- Cada módulo precisa de um `id` exclusivo. O gerenciador detecta sobreposição dos endereços de memória entre módulos ativos.
- Os campos em `parameters` são apresentados automaticamente na interface. Não precisa modificar `manager.py` para criar um módulo novo.

**Não é um importador universal de tabelas `.CT`.** Os três arquivos enviados foram convertidos previamente. Scripts novos com assembly ou injeção de código precisam de conversão para o formato JSON; macros, Lua, AOB scanning e `createThread` do Cheat Engine não são executados pelo gerenciador. O formato e as ferramentas para criar novos módulos estão em `CRIAR-PLUGINS.md`.

## Restauração e erros

Desativar restaura instruções e os valores capturados; não desfaz consequências de jogo já ocorridas, como dano, morte, dinheiro recebido ou veículos criados. O código de uma chamada já iniciada pode concluir antes de parar.

Antes de alterar código, o gerenciador pausa brevemente as threads do servidor e verifica se elas não estão executando dentro das regiões afetadas. Se estiverem, cancela a operação e pede nova tentativa. As threads são retomadas no fim da operação. As regiões de código injetado permanecem alocadas até o servidor encerrar, evitando liberar memória que possa ter retornos pendentes. Existe um limite de 256 blocos alocados por sessão; a DLL unificada usa dois blocos por ativação.

Se um outro programa alterar um trecho instalado, a desativação é recusada em vez de sobrescrever alterações desconhecidas. Se uma restauração falhar, a sessão é bloqueada e o servidor precisa ser reiniciado. Evite ligar o Cheat Engine nos mesmos pontos enquanto o gerenciador estiver conectado.

Erros completos ficam em `logs/manager.log`. O hash do executável precisa corresponder à versão 024d enviada. Os endereços são calculados usando a base real do módulo, inclusive quando o Windows o carrega em outro endereço.

## Verificação realizada e herdada

- 304 verificações nativas em emulação da ejeção, em duas bases de memória: limites dos pools, corpos inativos, veículo modelo 11, donos inválidos, retorno à rotina original com mesmos argumentos/registros/flags, dano nativo e restauração. Sem execução de uma partida Windows ou teste de rede com cliente.

- 12 testes da organização: migração, ordenação, movimentação entre pastas, arrastar/soltar, criação, renomeação, exclusão sem apagar scripts, estado aberto/fechado e restauração após erro de gravação. Testados com uma árvore simulada; sem execução visual no Windows.

- 98 verificações em emulação da rotina nativa do motor: curva de força, faixa de rotação, multiplicadores, outros veículos e restauração, com duas bases de memória. Sem medição em uma partida Windows.

- 73 testes automatizados do motor, protocolo de pedidos e suporte a DLLs: parâmetros, restauração de todos os módulos liberados, conflitos, falha durante escrita, alterações externas e endereços base diferentes.
- 102 imagens de seções comparadas com o GNU linker, para conferir as relocações em duas bases distintas do servidor.
- 32 verificações nativas da versão 0.3.0, herdadas sem mudança no código do spawn: criação e ligações de corpo/IA, posição e direção, eventos de criação, pedidos repetidos, corpo morto, jogador no carro, desconexão, pools cheios e preservação de seis valores na pilha x87.
- 422 verificações nativas da DLL unificada em duas bases. Incluem `/imortal`, dano letal nativo, impactos e sangramento da faca com vida em 100, preservação da pilha x87, troca de corpo/conexão, fim de partida, desativação e reconfiguração da DLL.
- Verificação de sintaxe dos arquivos Python.

**Não foi possível executar o servidor ou testar uma partida no Windows neste ambiente.** Os testes do motor usam memória simulada; o teste nativo executa código x86 real em um mundo sintético, com funções CRT substituídas. Não comprovam a física num mapa carregado, a renderização/replicação em clientes, a interface nativa ou as chamadas da API do Windows. Esta é uma versão experimental para teste.

Código-fonte completo, módulos JSON e tabelas originais acompanham o pacote.

## Histórico

O script separado de spawn foi arquivado em `tools/npc_spawner-archived.json`, fora da lista de módulos. O comando `/npc` substitui seu uso.

A análise estática da DLL enviada está em `ANALISE-BLACK-MARKET.md`. Na entrega 0.3.1, mercado e chat ainda não tinham sido convertidos. Na 0.4.0, o chat foi reimplementado em uma DLL independente; Na 0.5.0, o mercado foi reimplementado com compra por telefone, bandagem e ataque de faca simplificado; os extras da DLL antiga não foram incorporados.
## Caminhão na concessionária — validação da 0.6.7

Ative o módulo separado **Caminhão na concessionária** e abra a lista de compra na concessionária. O caminhão é a oferta nativa de veículo 9, com preço padrão $12000. Após uma compra, o próximo ciclo repõe uma oferta. Catálogos completos perdem somente a última oferta para abrir espaço. Nenhum veículo físico é criado pela reposição; a compra segue a rotina normal do jogo.

Passou em 56 testes do gerenciador e 34 verificações em emulação em duas bases, cobrindo estoque vazio/completo, preço personalizado, reposição, ausência de duplicação, índices inválidos, compra e dinheiro insuficiente. A rotina de compra foi executada com chamadas de criação de veículo/chave simuladas. Rede e cliente Windows não foram executados; ainda é necessária a confirmação na partida.

O módulo preserva os dois valores globais da tabela original: preço inteiro 12000 e propriedade float 5000 no RVA 0xAC9B2DC. Ao desativar, o gerenciador restaura os valores que existiam antes da ativação. As ofertas de catálogo existentes ficam sujeitas à atualização normal do servidor, e veículos comprados continuam no mapa.
As respostas privadas dos comandos resolvem o corpo atual do jogador a cada envio. Se o admin ainda não tiver um corpo vivo, a resposta fica no console; o cadáver não recebe a mensagem.
