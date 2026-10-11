# Criar e configurar módulos

Os arquivos `plugins/*.json` são carregados sem alterar o código do gerenciador. Eles contêm bytes de patch e, quando necessário, código x86 convertido previamente. São específicos do servidor 024d cujo hash está em `core.py`.

## Plugins em DLL (a partir da 0.4.0)

O formato de patches abaixo continua funcionando. Para implementar lógica nativa mais complexa em C/C++ e carregá-la como DLL, consulte `CRIAR-PLUGINS-DLL.md` e o exemplo compilado `plugins/native_combined.json`.

## Exemplo de módulo simples

Este exemplo muda o valor inicial usado pela tabela do dinheiro. É equivalente ao módulo incluído; use apenas um deles de cada vez, pois ambos alteram o mesmo endereço.

```json
{
  "format": 1,
  "id": "dinheiro_personalizado",
  "name": "Meu dinheiro inicial",
  "description": "Valor personalizado para a rotina de inicialização.",
  "sections": [{
    "name": ".patch_e027",
    "rva": 57383,
    "data": "c782947b430f88130000",
    "expected": "c782947b830fe8030000",
    "expected_relocations": [2],
    "relocations": [{
      "offset": 2, "type": 1,
      "symbol": {"name": "__server", "value": 0, "section": "", "index": 0}
    }]
  }],
  "parameters": {
    "money": {
      "label": "Dinheiro inicial", "type": "int", "default": 5000,
      "min": 0, "max": 100000000,
      "sites": [{"section": ".patch_e027", "offset": 6}]
    }
  },
  "imports": {}, "symbols": []
}
```

`rva` é o deslocamento em relação à base real do executável, escrito como número decimal no JSON. `data` contém os bytes a instalar; valores de endereço que precisam da base do servidor são codificados como deslocamentos, com uma relocação `__server`. `expected` contém os bytes originais da imagem no disco na base preferida 0x400000. `expected_relocations` lista os campos de 4 bytes que o Windows ajusta ao carregar a imagem em outra base.

Para valores de dados que mudam naturalmente durante a sessão, `expected` pode ser `null`: nesse caso o gerenciador captura o valor vivo antes da alteração. Use isso somente para dados, nunca para ignorar divergências de código. O gerenciador sempre verifica que o valor instalado ainda pertence ao módulo antes de restaurá-lo.

## Parâmetros

- `type`: `int` (inteiro de 4 bytes), `float` (IEEE 754 de 4 bytes), ou `byte` (1 byte).
- `default`, `min`, `max`: valor inicial e limites aceitos pela interface.
- `sites`: uma ou mais posições a substituir. O mesmo parâmetro pode aparecer em vários pontos.
- Para um byte, indique `"width": 1` no site. Inteiros e floats usam largura 4 por padrão.
- `scale` opcional multiplica o valor antes da escrita; os parâmetros negativos de empurrão e limiar usam -1.

Os parâmetros são validados e aplicados antes de instalar os hooks. Não são congelados continuamente. Para mudanças durante a partida, desative o módulo e ative com os novos valores.

## Código injetado

A seção `.inject` é alocada no processo como código. A seção opcional `.inject_data` é alocada separadamente como dados graváveis, para estado e comandos; o código permanece sem permissão de escrita após a instalação. As demais seções usam seus RVAs. Os símbolos internos e relocações usam o formato ELF32 produzido pelo GNU assembler:

- Tipo 1 (`R_386_32`): `S + A`.
- Tipo 2 (`R_386_PC32`): `S + A - P`.
- `S`: endereço resolvido do símbolo; `A`: valor de 4 bytes já presente no local; `P`: endereço do local de relocação.
- Símbolo indefinido `__server`: base do executável.
- Símbolo de seção: endereço da seção mais `value`.
- Símbolo com `index=65521`: constante absoluta.
- Importação: nome listado em `imports`, cujo valor é o RVA de um slot IAT de 32 bits. O motor lê o ponteiro real desse slot. Os modos de morte são DLLs e não usam essa importação.

O módulo não pode conter patches internos sobrepostos. Patches ativos de módulos distintos também não podem se sobrepor. A restauração usa uma captura dos bytes reais e não o bloco `[DISABLE]` da tabela.

## Ferramentas de desenvolvimento incluídas

**Não são necessárias para executar o gerenciador.** Para regenerar os três conjuntos originais: Python com `pefile` e GNU binutils (`as`, `ld`, `objcopy`, `readelf`), por exemplo em Linux ou WSL. Execute a partir da pasta do pacote:

```bash
python tools/convert_tables.py /caminho/subrosadedicated024d.exe
python tools/finalize_plugins.py /caminho/subrosadedicated024d.exe
python -m unittest discover -s tests -v
python tools/verify_linking.py
```

`convert_tables.py` é uma conversão específica destas tabelas, com a sintaxe usada nelas. Não é um compilador geral do Cheat Engine. `finalize_plugins.py` adiciona parâmetros, metadados de ASLR e bloqueia os dois módulos ainda não portados.

Para criar um módulo novo em assembly GNU Intel, `tools/build_plugin.py` aceita seções `.inject` e `.patch_RVA`, extrai as relocações e obtém os bytes originais do executável. Configure os parâmetros no JSON resultante e confira os trechos e seus tamanhos antes de usar. Instruções sobrescritas devem ser completas; um jump de 5 bytes não pode substituir uma instrução de 2 bytes sem tratar as instruções seguintes.

```bash
python tools/build_plugin.py meu_script.s /caminho/subrosadedicated024d.exe meu_script "Meu script"
```

Exemplo de assembly sem alocação que altera o dinheiro inicial:

```asm
.intel_syntax noprefix
.code32
.section .patch_e027,"ax"
mov DWORD PTR [edx+__server+0xF437B94],5000
```

Módulos JSON permitem modificar código do seu servidor. Revise os endereços, a lógica e o código de qualquer módulo que você adicionar. Não estão isolados em uma sandbox.


## Pedidos executados no loop principal (0.2.0)

O módulo `npc_spawner.json` usa `command` para adicionar um botão à interface. O protocolo atual é uma fila de um pedido com cinco inteiros de 32 bits no início de `.inject_data`: pedido, quantidade, restante, criados e status. O handler atualiza esse bloco no loop do servidor; o gerenciador apenas envia e lê os pedidos. A 0.1.0 não oferece esse protocolo, portanto módulos com comandos requerem a 0.2.0 ou posterior. A versão 0.3.0 substitui a implementação do spawn por NPC sem veículo.

`tools/npc_spawner.s` contém o novo código analisável. `tools/build_npc_spawner.py CAMINHO_DO_SERVIDOR.exe` recompila o módulo e seus metadados. Essa ferramenta exige GNU binutils e `pefile`, como as demais ferramentas de desenvolvimento; não é necessária para usar o módulo pronto.

Verificação nativa da versão 0.3.0 (desenvolvimento, em Linux com GCC/binutils, `pefile` e `unicorn`):

```bash
python3 tools/verify_native_spawn.py /caminho/subrosadedicated024d.exe
```

O script usa temporários para montar uma imagem do executável fornecido e executar um harness x86 emulado. Nenhum executável do servidor acompanha este pacote. O mundo, o retorno de `rand` e as dependências CRT do teste são sintéticos; não há execução do servidor Windows ou de clientes.
