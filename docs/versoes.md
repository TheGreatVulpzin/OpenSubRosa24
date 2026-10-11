# Versões dos binários

## Resumo

| Binário | Data de build | Papel |
| --- | --- | --- |
| `client/subrosa.exe`, `subrosa.x64`, Mac | 2015-08-02 | Cliente usado pela galera. |
| `client/subrosadedicated.exe` | 2015-08-02 | Dedicado do mesmo build do cliente. |
| `blackhouser/subrosadedicated024d.exe` | 2015-09-05 | Dedicado "0.24d" do Black Houser. Alvo do server manager. |

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

## Pendências

- Achar o número de versão do protocolo. O cliente mostra a versão com o formato `ALPHA /i`.
- Comparar a tabela de tipos de veículo (`setupobjecttypes_*`) entre o ELF e o 024d.
