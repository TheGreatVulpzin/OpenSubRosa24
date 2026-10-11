# Plugins nativos em DLL — ABI 1

O gerenciador 0.6.3 pode carregar módulos C/C++ de 32 bits no servidor dedicado 024d. A lógica fica na DLL; o JSON descreve configuração, símbolos exportados e regiões de código que o gerenciador pode instalar/restaurar. As DLLs usam o mesmo sistema de conflito, relocações ASLR e ordem da lista dos patches atuais.

## Exemplo pronto: módulo unificado

| Arquivo | Função |
| --- | --- |
| `plugins/native/sr_combined.dll` | DLL x86 pronta |
| `plugins/native_combined.json` | Contexto, parâmetros e três hooks |
| `native_sdk/black_market.c` | Mercado, faca e ciclo de configuração/parada |
| `native_sdk/combined_commands.h` | Chat e comandos admin |
| `native_sdk/extra_commands.h` | Aparência, teleporte, bases e som |
| `native_sdk/combined.def` | Exportações públicas |
| `native_sdk/black_market_hook.s` | Wrappers de tick com preservação de FP |
| `tools/build_market_dll.py` | Compila a DLL unificada e seu manifesto |
| `tools/verify_market_dll.py` | Exercita a DLL compilada em emulação x86 |

O callback de chat registra a mensagem, trata comandos conhecidos e encaminha as demais mensagens à função original. A trampoline preserva sete bytes da entrada de chat e retorna ao RVA 0x49947.

## Criar outro plugin

1. Implemente uma DLL x86 e exporte duas funções de controle com assinatura `DWORD WINAPI Nome(LPVOID contexto)`. Não use DLL x64 nem DLL .NET.
2. Exporte também um `const char` com 65 bytes: 64 caracteres hexadecimais de Build ID e terminador NUL. Atualize o Build ID quando mudar código, estrutura do contexto ou opções de compilação.
3. Exporte as funções que serão chamadas pelas regiões de hook. Use `extern "C"` em C++ e nomes explícitos no `.def` para manter os símbolos estáveis. A convenção de chamada do hook precisa coincidir com a função de jogo substituída; o exemplo usa `__cdecl` para o chat e `WINAPI/__stdcall` para Configure/Stop.
4. Prepare um JSON com `kind: "dll"`, `dll.abi: 1`, caminho relativo, SHA-256 do binário, Build ID, nomes das exportações e seções de patch. Comece pelo manifesto do exemplo.
5. Reserve `.inject_data` para o contexto. Defina `context_section` e, se necessário, `context_offset`. Relocações podem preencher ponteiros para o servidor e trampolines. Parâmetros numéricos continuam usando `sites` para preencher o contexto.
6. Descreva todos os hooks em `sections`, com os bytes `expected` e relocações corretas. O gerenciador precisa conhecer cada região alterada para verificar conflitos e restaurá-la.
7. Importe o JSON com a DLL principal ao lado, no caminho relativo indicado. Bibliotecas auxiliares são copiadas manualmente para a subpasta da DLL. Teste primeiro cada hook/feature separadamente.

Não há limite de complexidade baseado no tamanho de um script Cheat Engine: a DLL pode organizar estados, callbacks, eventos e funções em C/C++. Isso não elimina os limites do jogo: pools de personagens/itens, estruturas e APIs continuam específicos da versão 024d. Recursos como mercado, inventário, NPCs e regras de combate exigem código e testes próprios. Conteúdo visual novo que o cliente não conhece pode precisar de modificações nos arquivos do cliente também.

## Campos nativos do manifesto

Use o manifesto real `plugins/native_combined.json` como referência completa.

| Campo em `dll` | Significado |
| --- | --- |
| `abi` | `1`, versão do contrato do gerenciador |
| `file` | Caminho relativo à pasta plugins, como `native/sr_combined.dll` |
| `sha256` | SHA-256 dos bytes da DLL pronta |
| `build_id` | Identificador de compilação de 64 caracteres hexadecimais |
| `identity` | Exportação de dados que contém o Build ID com NUL |
| `configure` | Exportação de inicialização/configuração |
| `stop` | Exportação de parada cooperativa |
| `symbols` | Mapa de aliases de relocação para funções exportadas |
| `context_section` | `.inject_data` |
| `context_offset` | Deslocamento do contexto; padrão `0` |

Um símbolo indefinido, por exemplo `__dll_chat`, é resolvido para a função exportada em `symbols`. `__server` continua significando a base real do executável. Não use aliases que colidam com `__server` ou com imports do servidor. Exportações encaminhadas não são aceitas nas DLLs de plugins; o backend suporta encaminhamentos nas DLLs do Windows usadas para carregamento.

## Contexto do exemplo

| Offset | Tipo | Valor |
| --- | --- | --- |
| 0 | DWORD | Tamanho: 80 |
| 4 | DWORD | ABI: 1 |
| 8 | DWORD | Base do servidor |
| 12 | DWORD | Ponteiro da trampoline original de chat |
| 16 | LONG | Flag enabled |
| 20–32 | 4 int32 | Preços e intervalos de compra |
| 36–56 | 6 float | Coordenadas dos dois pontos |
| 60 | int32 | Dano da faca |
| 64 | float | Alcance |
| 68 | int32 | Diagnóstico |
| 72 | int32 | Mostrar ID no chat do console |
| 76 | int32 | Habilitar comandos admin |

ABI 1 define o procedimento de controle, não um único formato universal para todos os contextos: outra DLL pode estender esse cabeçalho e documentar seus próprios campos. O gerenciador exige pelo menos 16 bytes na região do contexto, mas a DLL precisa validar seu tamanho, versão e valores específicos.

Se usar também o protocolo `command` já existente, reserve os primeiros 20 bytes de `.inject_data` para a fila de pedido; o contexto da DLL deve começar no offset 20 ou maior. Esse protocolo/feedback da interface ainda está orientado aos pedidos de NPCs, não constitui uma interface genérica de comandos de DLL.

## Ciclo de ativação e parada

- **Ativar:** valida o manifesto, hash, arquitetura, exportações e Build ID; carrega/reutiliza a DLL; prepara memória e parâmetros; chama Configure fora da pausa de threads; instala os hooks conhecidos com as threads brevemente pausadas.
- **Configure:** recebe um ponteiro ao contexto no servidor e retorna `0` em sucesso. É executada numa thread auxiliar; prepare estados/configuração, sem chamar ali rotinas de jogo que exigem a thread principal. O trabalho de jogo deve ocorrer no callback executado pelo hook na thread original.
- **PrepareStop opcional:** um manifesto pode indicar `"prepare_stop": "SR_PrepareStop"`, com assinatura DWORD WINAPI(LPVOID). Antes de restaurar o hook, o gerenciador verifica seus bytes sob pausa, retoma as threads e chama essa exportação. Ela pode pedir limpeza no callback da thread do jogo. Retorno não zero conhecido mantém o hook/estado para nova tentativa; timeout remoto incerto bloqueia a sessão. A exportação é validada como código no PE local e no módulo residente.
- **Desativar:** verifica/restaura as instruções originais, retoma as threads e chama Stop fora da pausa. Stop retorna `0` quando a feature parou cooperativamente. Chamadas já iniciadas podem concluir.
- **Stop:** sinalize a interrupção de workers próprios e encerre-os cooperativamente, respeitando chamadas em andamento. Não libere contextos/trampolines do gerenciador nem chame FreeLibrary/FreeLibraryAndExitThread. O exemplo somente desliga uma flag e não possui worker permanente.
- **Residente:** a DLL e as alocações de contexto/trampoline ficam no servidor até o processo encerrar. Ativar/desativar o mesmo recurso não descarrega a DLL. Um contexto antigo pode continuar em uso por callbacks em andamento; não reutilize nem sobrescreva essa memória.
- **Falha:** Configure recusada deixa o hook original e chama Stop. Falha de restauração, parada não confirmada ou chamada remota sem resultado conhecido bloqueia a sessão; reinicie o servidor. A chamada remota possui espera de 5 segundos, sem TerminateThread, e seus dados permanecem válidos após um timeout.

O DllMain deve ser mínimo. Os hooks são instalados pelo gerenciador: não instale patches ocultos no DllMain/Configure e não remova seu próprio hook durante callbacks. Assim o gerenciador consegue rastrear os conflitos e a restauração. Este é um contrato para DLLs desenvolvidas para o SDK, não uma verificação automática de tudo que uma DLL arbitrária poderia fazer.

## Compilar o exemplo

Para usar o pacote pronto, não precisa recompilar. O build do mercado também usa GNU `as --32` para a trampoline ELF (desenvolvimento em Linux/WSL). Para desenvolver, instale MinGW-w64 **i686** com `i686-w64-mingw32-gcc` no PATH, ou passe seu caminho:

```text
py -3 tools/build_market_dll.py --server C:/servidor/subrosadedicated024d.exe
py -3 tools/build_market_dll.py --server C:/servidor/subrosadedicated024d.exe --cc C:/mingw32/bin/i686-w64-mingw32-gcc.exe
```

Também há `native_sdk/Compilar-exemplo.bat`. O build gera a DLL e atualiza o JSON juntos. O exemplo usa somente KERNEL32, sem CRT adicional. DLLs mais complexas podem ter outras dependências, que devem ser fornecidas e compatíveis com x86. O carregador usa LoadLibraryExW com o diretório da DLL incluído na busca de dependências, além dos diretórios padrão do Windows.

Feche/reinicie o servidor antes de substituir uma DLL já carregada. Desativar permite alterar parâmetros e ativar novamente; atualizar o binário é uma operação diferente.

## DLL antiga do Black Market

A `blackmarketDedicated_2.dll` não exporta as funções do SDK e instala seis hooks por conta própria. Ela não pode ser usada diretamente como plugin ABI 1. O relatório `ANALISE-BLACK-MARKET.md` continua sendo a referência para converter o mercado e os comandos adicionais. A 0.6.3 inclui somente o módulo unificado `native_combined.json`; consulte `BLACK-MARKET.md` e `COMANDOS-DLL.md`.

Não use as duas DLLs de chat ao mesmo tempo: a verificação dos bytes esperados recusa o hook se a DLL antiga já alterou aquela rotina.

## Verificação desta entrega

52 testes Python do gerenciador; 232 verificações nativas da DLL unificada em duas bases; 102 imagens de seções comparadas com o GNU linker. A DLL unificada é idêntica à 0.6.2, que acrescentou os comandos extras. APIs Windows emuladas.

Para repetir as verificações nativas de desenvolvimento, instale `pefile` e `unicorn`:

```text
python tools/verify_market_dll.py /caminho/subrosadedicated024d.exe
python -m unittest discover -s tests -v
```

## Referências de implementação

- [Microsoft: CreateRemoteThread e direitos do processo](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createremotethread)
- [Microsoft: LoadLibraryExW e busca de dependências](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw)
- [Microsoft: restrições do DllMain e loader lock](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-best-practices)
- [Microsoft: formato PE e tabela de exportações](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)


A DLL de morte em `native_sdk/death_messages.c` é compilada com `tools/build_death_plugins.py SERVIDOR --cc COMPILADOR`. O builder valida o executável 024d e gera os três modos exclusivos. O contexto tem seis DWORDs: tamanho (24), ABI (1), base do servidor, reservado, enabled e mode (1: mensagens; 2: instantânea; 3: ambas). Os hooks preservam os registradores e o estado de ponto flutuante. `SR_PrepareStop` desabilita callbacks e espera as chamadas em curso terminarem; a DLL permanece residente conforme o protocolo do gerenciador. Para verificar os hooks e o processamento nativo de morte/mensagens: `python tools/verify_death_dll.py SERVIDOR` (dependências de desenvolvimento: pefile e unicorn).
