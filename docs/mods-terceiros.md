# Mods de terceiros no cliente

O cliente da galera tem otimizações fanmade. Elas estão em DLLs que não são da Cryptic Sea.

## `client/wsock32.dll`

| Campo | Valor |
| --- | --- |
| SHA-256 | `99d3fadae0e3b24f0cb24c664fd5e84f5710bfa94a5e4d61a6d785fc483646dd` |
| Build | 2023-03-14, MSVC 2022 (linker 14.34), C++ |
| Tipo | DLL proxy. Exporta as 75 funções do `wsock32.dll` do Windows. |

Como funciona (análise estática das importações e strings):

1. O `subrosa.exe` importa `WSOCK32.dll`. O Windows carrega primeiro a cópia da pasta do jogo.
2. A DLL busca o `wsock32.dll` real em `GetSystemDirectoryW` e repassa as chamadas de rede.
3. A DLL usa `VirtualProtect` cerca de 23 vezes. Cada chamada abre um trecho do jogo para escrita. São os patches.
4. Strings do jogo dentro da DLL: `conveniencestore1`, `texture/cash_thousand.png`. Os patches mexem pelo menos na loja de conveniência e na textura de dinheiro.

Indícios de segurança:

- Não importa funções de internet, processos, registro ou escrita de arquivos.
- Só importa funções de memória, carga de DLL e CRT.
- Conclusão parcial: baixo risco. A lista exata de patches precisa da decompilação.

## `client/SDL2.dll`

Arquivo de 2024-02-21. O jogo original é de 2015. Alguém trocou o SDL2 por uma versão mais nova.

## Pendências

- Decompilar o `wsock32.dll` no Ghidra e listar cada patch (endereço, bytes, efeito).
- Ver se algum patch muda o protocolo de rede. Se mudar, o servidor novo precisa saber.
- Descobrir a versão do `SDL2.dll`.
