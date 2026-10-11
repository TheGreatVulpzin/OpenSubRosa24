# Análise de blackmarketDedicated_2.dll

Análise estática do arquivo fornecido, sem executar ou injetar a DLL. As conclusões abaixo vêm das instruções x86, das constantes, das strings e das importações do próprio arquivo. Não constituem teste dentro do jogo.

## Identificação

- Arquivo: `blackmarketDedicated_2.dll`, 32.256 bytes.
- SHA-256: `5c9add9ae7abc0f2172d5e21dc0df877f42af31bc43c4ed88bcaebe65baf61db`.
- Formato: DLL Windows PE de 32 bits, x86, C++ com runtime Microsoft.
- Base preferencial: `0x10000000`. Os endereços de DLL citados abaixo usam essa base; RVAs do servidor são relativos à base real do executável.
- Usa `GetModuleHandleA(NULL)` para obter a base do servidor e soma endereços fixos de funções e estruturas. Vários desses RVAs coincidem com os que já foram identificados no executável dedicado 024d fornecido. Isso não garante compatibilidade com outra versão.

## Mercado: o que foi encontrado

A DLL cria **dois itens do tipo 23 (`0x17`)**, usados como telefones de compra, quando o contador lido em `servidor + 0x90DC578` é exatamente **450**. O hook fica na rotina `servidor + 0x5B9B0`; a implementação da DLL começa em `0x100023B0`.

| Ponto | X | Y | Z |
| --- | ---: | ---: | ---: |
| 1 | 692 | 25,725 | 354 |
| 2 | 1220 | 25,725 | 680 |

São coordenadas internas do mapa, extraídas das constantes da DLL. Não identifiquei o nome desses locais na interface do jogo. A DLL grava essas posições nos campos `item + 0x170`, `+0x174`, `+0x178`; durante a atualização ela mantém os itens fixos ali.

| Compra | Número discado | Preço | Tipo do item criado | Espera após compra |
| --- | ---: | ---: | ---: | ---: |
| Faca | 24 | $125 | 24 (`0x18`) | 400 atualizações |
| Bandagem | 12 | $70 | 12 (`0x0C`) | 200 atualizações |

As esperas são contadores decrementados na rotina de atualização, não durações em segundos confirmadas. A string do menu é `Faca: $125 (Disque 24) / Bandagem: $70 (Disque 12)`.

A compra consulta o saldo em `player + 0x40`, compara com o preço e subtrai o valor depois de chamar a função de criação de item em `servidor + 0x1B070`. A posição usada para criar o item é a do telefone. Há mensagens para dinheiro insuficiente e nova tentativa durante a espera. A rotina também emite eventos de som/efeito por `servidor + 0x16DB0`.

O processamento aparece entre `0x1000339B` e `0x10003767`. Ele verifica tipo 23, item ativo e estado especial 3 em `item + 0x38`; portanto a lógica observada distingue os telefones especiais dos telefones comuns. O número discado está em `item + 0x108`, a espera em `+0x158` e o estado próprio do mod em `+0x160`. A análise posterior do servidor confirmou que o estado nativo de ligação fica em `+0x110`; não são o mesmo campo.

O gatilho de criação depende de atingir exatamente o contador 450. Se a DLL for carregada depois desse ponto, esse trecho não cria os telefones imediatamente; seria necessário observar um reinício do contador ou adaptar o gatilho na conversão.

## Chat no console

A DLL substitui a entrada da rotina de chat em `servidor + 0x49940` por um salto para seu handler em `0x10002770`.

Para jogadores ativos, o trecho `0x100027C2–0x100027E7` escreve no `std::cout`:

```text
Nome do jogador: mensagem
```

O nome vem de `player + 4`; a mensagem vem do segundo argumento da rotina. O registro acontece antes da condição de permissão usada pelos comandos adicionais. Portanto não está limitado às mensagens de quem pode executar esses comandos. Depois o wrapper chama a rotina original do servidor.

Não encontrei uma função de gravação dessas mensagens em arquivo: o comportamento identificado é impressão no console. A DLL também altera o título da janela para `Sub Rosa Dedicated Server` e a cor do texto do console.

## Outros recursos presentes

Além do mercado e do registro de chat, o handler reconhece estas strings de comando:

```text
/heal
/kill
/item
/vehicle
/arm
/heavydude
/delh
/team
/deactivate
```

Há uma condição `player + 0x2C != 0` antes do processamento desses comandos, aparentemente ligada à permissão do jogador; o significado exato desse campo não foi confirmado nesta análise. Os handlers incluem alterações em personagens, criação de itens e veículos, troca de equipe e ativação/desativação de registros de jogadores. A rotina de atualização também contém alterações relacionadas a faca, personagens e veículos. Esses extras precisam de análise e testes próprios antes de conversão; não devem ser incorporados automaticamente ao mercado.

## Pontos de integração identificados

| Recurso | RVA no servidor | Handler na DLL |
| --- | --- | --- |
| Gatilho que cria os telefones | `0x5B9B0` | `0x100023B0` |
| Alocação de jogador | `0x28020` | `0x10002580` |
| Wrapper de rotina com quatro argumentos | `0x143B0` | `0x10003880` |
| Remoção de jogador | `0xA1B0` | `0x10002670` |
| Chat e comandos | `0x49940` | `0x10002770` |
| Atualização de humanos e mercado | `0x51060` | `0x10002DA0` |

Estruturas usadas: jogadores em RVA `0xEDEDD00`, tamanho `0x2408`; humanos em `0x15CBF80`, tamanho `0x2B64`; itens em `0x91022C0`, tamanho `0x1AC`; veículos em `0x5592600`.

## Como aproveitar no gerenciador

Os trechos identificados permitem planejar dois módulos independentes:

1. **Chat no console:** hook de chat que imprime nome e mensagem e preserva o comportamento original. Os comandos extras ficam fora desse módulo.
2. **Black Market:** criação explícita e controlada dos pontos, coordenadas e preços configuráveis, validação do saldo, criação dos itens e espera entre compras.

Para ativar/desativar com o servidor aberto, a conversão deve tratar os itens já criados e restaurar os hooks sem interromper uma chamada em andamento. O gerenciador deve verificar conflitos com outros módulos e com a própria DLL antes de alterar os mesmos endereços.

A DLL original instala saltos de cinco bytes e, em vários wrappers, retira temporariamente o hook para chamar o original e depois o reinstala. Esse mecanismo precisa ser adaptado ao controle de threads e à restauração usados pelo gerenciador. Seu thread auxiliar consulta continuamente a tecla End sem pausa; ao detectar a tecla, chama `FreeLibraryAndExitThread`. Não há restauração explícita dos hooks nesse caminho nem no DllMain observado. A eventual limpeza por destruidores do runtime ainda precisaria ser auditada antes de confiar no descarregamento durante execução.

## Entrega 0.3.1

Foi alterado somente o nome exibido do módulo para **Spawnar NPC**, junto com a versão e a documentação. O código de spawn que você confirmou funcionando foi preservado. O mercado e o chat acima foram analisados, mas ainda não foram implementados como novos módulos nem testados no servidor Windows.

## Conversão na 0.5.0

O mercado agora está reimplementado em `sr_black_market.dll`, com ciclo de limpeza controlado pelo gerenciador. Consulte `BLACK-MARKET.md` para diferenças, endereços confirmados e limites dos testes. Os extras e comandos da DLL antiga não foram incorporados.
