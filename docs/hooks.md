# DLL de hooks (Fase 2)

**A Fase 2 está concluída.** O método funciona de ponta a ponta no servidor real: DLL carregado, ASLR tratado, hook instalado, original chamada pelo trampolim e resultado idêntico.

O SubVulpes troca funções do `subrosadedicated024d.exe` por versões em C, uma de cada vez.
O mecanismo é um `wsock32.dll` proxy na pasta do servidor.

## Como funciona

1. O 024d importa `WSOCK32.dll`. O Windows carrega primeiro a cópia na pasta do executável.
2. O nosso `wsock32.dll` carrega o `wsock32.dll` do sistema e repassa as 47 funções de rede.
3. Ele confere se o executável é o 024d (data de build e tamanho do PE). Se não for, só repassa a rede.
4. Ele instala os hooks: um `JMP` de 5 bytes no início de cada função original, para a versão em C.
5. Um trampolim guarda o início da função original. Assim dá para chamar a original.

Código: [`hook/`](../hook/) (proxy, hooks, tabela) e [`src/server/`](../src/server/) (funções reescritas).

## Funções trocadas

| Função | RVA no 024d | Arquivo | Estado |
| --- | --- | --- | --- |
| `createevent_message` | `0x6260` | `src/server/events.c` | **Verificada** (2026-10-11): 28 chamadas com cliente conectado (boas-vindas, Top 10, corporações, entrada, chat), 0 diferenças. |
| `createevent_updatedoor` | `0x6660` | `src/server/events.c` | Lote 1. Compila. Falta testar no jogo. |
| `createevent_updatemission` | `0x6760` | `src/server/events.c` | Lote 1. Compila. Falta testar no jogo. |
| `fixplayername` | `0xa580` | `src/server/players.c` | Lote 1. Compila. Falta testar no jogo. |

Progresso: `python -I tools/progress.py`.

## Antes de reescrever uma função

1. **Confira o nome no código.** Os nomes do casador que vieram do grafo de chamadas erram com frequência. No lote 1, 4 de 7 nomes `createevent_*` estavam errados. Compare o pseudo-C do 024d com o do 0.24c (`build/decomp/`).
2. **Veja o prólogo:** `tools/ghidra/PrintPrologue.java` mostra as primeiras instruções. O prólogo do hook precisa ter instruções inteiras, pelo menos 5 bytes e nenhum salto relativo.
3. **Evite contas de float por enquanto.** O 024d usa x87. O nosso compilador usa SSE, e o último bit pode mudar.

## Modo de verificação

Com `verify=1` (padrão nesta fase), cada chamada roda as duas versões ([`hook/verify.c`](../hook/verify.c)):

1. A seção `.data` do jogo (todas as globais) fica somente leitura.
2. Roda a versão em C. Cada escrita gera uma exceção: o verificador guarda a página de 4 KB e libera a escrita.
3. Guarda o resultado e desfaz as escritas. Também restaura o estado do `rand()` do `MSVCR100.dll`.
4. Roda a original pelo trampolim, do mesmo jeito.
5. Compara só as páginas que alguma das versões alterou. Diferença vai para o `subvulpes.log`, com nome (exemplo: `event[1234]+0x40`).

Limites: escritas fora da `.data` (pilha, heap, outros DLLs) não entram na comparação. O servidor precisa rodar em uma thread só.

Teste do verificador sem o jogo: `build\hook\test_verify.exe` (9 casos).

O jogo continua com o resultado da **original**. Então um erro na versão em C não afeta a partida durante o teste.

## Compilar

```bat
tools\build_hook.bat
```

Saída: `build\hook\wsock32.dll`.

## Testar

1. Faça uma **cópia** da pasta do servidor (por exemplo, a pasta do Black Houser). Não teste na pasta que vocês usam todo dia.
2. Copie `build\hook\wsock32.dll` para a cópia, ao lado do `subrosadedicated024d.exe`.
3. Inicie o servidor como sempre.
4. Abra o `subvulpes.log` na mesma pasta. Ele deve ter estas linhas:
   - `SubVulpes carregado.`
   - `hook createevent_message instalado em +0x06260`
   - `1 hook(s) instalado(s)`
5. Conecte um cliente (você ou um amigo). Entre, saia e use o chat.
6. Confira no log as linhas `VERIFICA createevent_message: N chamadas, 0 diferenças`.

Para conectar de outro PC, o amigo precisa alcançar a porta UDP do servidor (`port=` no `config.txt`, padrão 27584). Tailscale ou ZeroTier servem.

## `subvulpes.ini` (opcional, na pasta do servidor)

```ini
[hooks]
enabled=1                ; 0 = só repassa a rede, sem hooks
verify=1                 ; 1 = compara com a original a cada chamada
createevent_message=1    ; 0 = desliga só este hook (vale para qualquer nome de hook)
verify_every.createevent_message=1   ; verifica 1 a cada N chamadas deste hook
log_calls=1              ; 1 = registra cada chamada verificada
```

Ao fechar o servidor, o log recebe uma linha `RESUMO` por função, com o total de chamadas e de diferenças.

## Remover

Apague o `wsock32.dll` da pasta do servidor. O 024d volta a usar o `wsock32.dll` do Windows.

## Compatibilidade com o manager

O hook troca só os 5 primeiros bytes do `createevent_message`. O manager chama `0x626d`, no meio da função, e continua funcionando. Se outro programa já tiver alterado o início de uma função, o hook não é instalado e o log mostra `ERRO ... bytes originais diferentes`.
