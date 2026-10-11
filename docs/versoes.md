# Versões dos binários

## Resumo

| Binário | Data de build | Papel |
| --- | --- | --- |
| `client/subrosa.exe`, `subrosa.x64`, Mac | 2015-08-02 | Cliente ALPHA 24, usado pela galera. |
| `client/subrosadedicated.exe` | 2015-08-02 | Dedicado do mesmo build do cliente. |
| `blackhouser/subrosadedicated024d.exe` | 2015-09-05 | Dedicado "0.24d" do Black Houser. Alvo do server manager. |

## Versões oficiais baixadas (`original/vanilla/`)

| Arquivo | Formato | Símbolos | Uso |
| --- | --- | --- | --- |
| `0.24c_Dedicated_Linux/.../subrosadedicated024c` | ELF x64 | **Sim** (461 funções) | Fonte principal de nomes para o 024d. |
| `0.24b_Dedicated_Linux/.../subrosadedicated024b` | ELF x64 | **Sim** (461 funções) | Segunda fonte de nomes. |
| `0.24c_Dedicated_Windows/.../subrosadedicated024c.exe` | PE32, MSVC | Não | Revisão anterior ao 024d, mesmo compilador. Ponte para o 024d. |
| `0.24b_Dedicated_Windows/.../subrosadedicated024b.exe` | PE32, MSVC | Não | |
| `0.24d_Dedicated_Windows/.../subrosadedicated024d.exe` | PE32, MSVC | Não | **Idêntico ao 024d do Black Houser** (mesmo SHA-256). O Black Houser roda o 024d sem modificação. |
| `0.24b/` | Cliente completo (Windows, Linux, Mac) | ELF e Mach-O com símbolos | Cliente 24b. |
| `0.25b_Dedicated_Linux/subrosadedicated025b` | ELF x64 | Não (stripped) | Versão seguinte. |
| `0.22/`, `0.22_Dedicated_Server/` | Cliente e servidor 0.22 | Mach-O com símbolos | Versão anterior. |

## Cliente da galera x servidor 024d

Os jogadores entram no Black Houser (024d) com o cliente da galera. No servidor aparecem helicóptero, trem e AK-47. Fonte: relato do usuário em 2026-10-11.

Conclusão: o cliente de 02/08 e o 024d usam o mesmo protocolo. **A compatibilidade de rede deve seguir o par cliente 02/08 + servidor 024d.**

## Diferenças de strings entre os dois dedicados

O 024d não tem várias strings que o dedicado de 02/08 tem. Isso **não** indica que as features faltam.

| Só no dedicado 02/08 | Só no 024d |
| --- | --- |
| Nomes de veículos `Turbo`, `Turbo S`, `Truck` | `/phone`, `/give`, `/list3` |
| Nomes de itens `AK-47`, magazines | `gametype=` |
| Rótulos de debug `rotorrpm`, `rotormass` | Rótulos de debug `bondsim`, `objectsim`, `packetbuild` |
| `data/city.sbc`, `data/block/%s.sbl` | `Goldmen Inc` + `%s open` |

Explicações prováveis, a confirmar no código:

- O servidor não precisa de nomes de exibição. Só o cliente mostra os nomes.
- Os rótulos de debug (medição de tempo por etapa) mudaram entre os builds.

O código do 024d é menor: 356 KB em 340 funções, contra 470 KB em 468 funções no dedicado de 02/08. O 024d usa mais inlining.

## Número de versão

O cliente guarda a versão na variável global `currentversion` (ELF, endereço `0x79731c`). O valor é **24**. O menu principal mostra `ALPHA 24`. A lista de servidores mostra `Alpha N` para cada servidor.

O servidor `024d` é a versão 24, revisão "d". Cliente e servidor são da mesma versão.

## Pendências

- Comparar a tabela de tipos de veículo (`setupobjecttypes_*`) entre o ELF e o 024d.
