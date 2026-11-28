# Documentação técnica — compilador C-

Este documento descreve a implementação desta branch e serve como guia rápido
para seguir a compilação desde o arquivo `.cm` até as quádruplas da GCI.

> **Limite de escopo:** esta branch termina na geração de código intermediário
> (GCI). A tradução das quádruplas para instruções da Tiny Machine não faz parte
> deste trabalho e não está implementada aqui.

## 1. Visão geral

O compilador é escrito em C11 e reconhece um subconjunto didático de C- baseado
na gramática de Louden. O fluxo de execução é:

```text
arquivo .cm
    │
    ▼
scanner.lx ── tokens e localização de linha
    │
    ▼
parser.y ──── AST
    │
    ▼
semantic.c ─ tabela de símbolos e validação
    │
    ▼
gci.c ─────── lista linear de quádruplas
    │
    ▼
main.c ────── imprime resultados e grava ast.dot
```

O parser constrói a AST durante o reconhecimento. A análise semântica e a GCI
percorrem essa mesma árvore; não há uma segunda árvore para o código
intermediário. As fases semântica e GCI só são executadas se o parser não
registrar erros.

## 2. Estrutura do repositório

| Caminho | Responsabilidade |
|---|---|
| [`src/main.c`](../src/main.c) | Ponto de entrada e coordenação das fases. |
| [`src/scanner.lx`](../src/scanner.lx) | Regras Flex: caracteres para tokens, comentários e erros léxicos. |
| [`src/parser.y`](../src/parser.y) | Gramática Bison, recuperação sintática e construção da AST. |
| [`src/ast.c`](../src/ast.c) / [`include/ast.h`](../include/ast.h) | Nós da AST, memória, impressão e exportação Graphviz. |
| [`src/symtab.c`](../src/symtab.c) / [`include/symtab.h`](../include/symtab.h) | Símbolos e escopos léxicos. |
| [`src/semantic.c`](../src/semantic.c) / [`include/semantic.h`](../include/semantic.h) | Declarações, tipos, chamadas, expressões e retornos. |
| [`src/gci.c`](../src/gci.c) / [`include/gci.h`](../include/gci.h) | AST para quádruplas de três endereços. |
| [`include/tokens.h`](../include/tokens.h) | Interface do scanner usada pelo restante do compilador. |
| [`Makefile`](../Makefile) | Geração do parser/scanner e compilação do executável. |
| [`tests/`](../tests/) | Programas C- de regressão, válidos e inválidos. |
| `build/` | Arquivos gerados pelo Bison e Flex. |

## 3. Compilar e executar

No Windows, a partir da raiz do repositório:

```powershell
mingw32-make
.\cmenos.exe .\tests\gcd.cm
```

O Makefile compila com `gcc -std=c11 -Wall -Wextra`, gera
`build/parser.tab.c` e `build/parser.tab.h` com Bison e gera
`build/lex.yy.c` com Flex. `mingw32-make clean` remove o executável, os arquivos
gerados em `build/` e `ast.dot`.

O programa recebe um único caminho de entrada:

```powershell
.\cmenos.exe caminho\para\programa.cm
```

Em caso de sucesso, a saída padrão contém, nesta ordem, a tabela de símbolos,
a AST em texto e as quádruplas. O programa também grava `ast.dot` no diretório
de execução. Erros e diagnósticos são enviados para `stderr`; erros de
compilação resultam em código de saída diferente de zero.

## 4. Linguagem reconhecida

O scanner reconhece:

- Palavras-chave: `else`, `if`, `int`, `return`, `void` e `while`;
- Identificadores compostos por letras (`[a-zA-Z]+`);
- Números inteiros compostos por dígitos;
- Operadores aritméticos `+`, `-`, `*`, `/`;
- Operadores relacionais `<`, `<=`, `>`, `>=`, `==`, `!=`;
- Atribuição `=` e delimitadores `; , ( ) [ ] { }`;
- Comentários de bloco `/* ... */`, sem aninhamento.

A gramática organiza programas em declarações globais, funções, parâmetros,
declarações locais, blocos e comandos. As expressões incluem atribuições,
operadores aritméticos e relacionais, identificadores, acesso a vetores,
chamadas e constantes. A precedência está codificada nas produções: `*` e `/`
agrupam em `term`; `+` e `-` em `additive_expression`; a comparação em
`simple_expression`; a atribuição é recursiva à direita.

As funções predefinidas são:

- `input()`: não recebe argumentos e produz um valor `int`;
- `output(int)`: recebe um argumento inteiro e retorna `void`.

As regras semânticas também exigem uma função `main` sem parâmetros, com retorno
`void`, como última declaração do programa.

## 5. Representação da AST

`ASTNode`, declarado em [`include/ast.h`](../include/ast.h), guarda o tipo de
nó, tipo de dado, nome, valor inteiro, linha de origem, até três filhos e um
ponteiro `sibling`. `sibling` representa listas, como sequências de declarações,
comandos, parâmetros ou argumentos. Os nós não guardam ponteiro para o pai.

Os filhos têm estes significados principais:

| Nó | Filhos |
|---|---|
| `NODE_FUN_DECL` | Parâmetros; corpo da função. |
| `NODE_COMPOUND_STMT` | Declarações locais; lista de comandos. |
| `NODE_IF_STMT` | Condição; ramo verdadeiro; ramo `else` opcional. |
| `NODE_WHILE_STMT` | Condição; corpo. |
| `NODE_RETURN_STMT` | Expressão retornada, se houver. |
| `NODE_ASSIGN_EXPR` | Destino; expressão atribuída. |
| `NODE_OP_EXPR` | Operando esquerdo; operando direito. |
| `NODE_ARRAY_EXPR` | Expressão do índice; o nome do vetor fica em `name`. |
| `NODE_CALL_EXPR` | Lista de argumentos; nome da função fica em `name`. |

As declarações distinguem variável (`NODE_VAR_DECL`), vetor
(`NODE_ARRAY_DECL`), função (`NODE_FUN_DECL`), parâmetro escalar
(`NODE_PARAM_VAR`) e parâmetro vetor (`NODE_PARAM_ARRAY`). As expressões
distinguem identificador, acesso a vetor, chamada, constante, operador e
atribuição. `line` permite associar diagnósticos à linha de origem.

### Funções de `ast.c`

| Função | O que faz |
|---|---|
| `allocation_failure` | Informa falha de memória durante a criação/visualização da AST e encerra o processo. |
| `type_name` | Converte `DATA_INT`/`DATA_VOID` para `int`/`void`. |
| `ast_copy_string` | Aloca e copia uma string; encerra com diagnóstico se a alocação falhar. |
| `new_node` | Cria um nó zerado e inicializa tipo e linha. |
| `ast_set_name` | Substitui o nome do nó por uma cópia alocada. |
| `ast_add_child` | Anexa um filho; acusa erro interno se exceder três filhos. |
| `ast_child` | Retorna um filho pelo índice ou `NULL` se o índice não existir. |
| `append_sibling` | Acrescenta um nó ao fim da lista ligada por `sibling`. |
| `free_ast` | Libera recursivamente filhos, irmãos, nome e nó. |
| `print_node_label` | Escreve o rótulo textual de um nó. |
| `child_forest_count` | Conta filhos e listas de irmãos para desenhar os conectores da árvore. |
| `getHeight` | Calcula a altura máxima da árvore e das listas de irmãos. |
| `reserve_ancestors` | Reserva o buffer auxiliar usado para desenhar ancestrais na impressão. |
| `fillBuffer` | Imprime recursivamente um nó e seus descendentes com conectores. |
| `print_tree` | Imprime `Program` e a AST no formato de árvore. |
| `printTree` | Atalho que chama `print_tree(stdout, root)`. |
| `print_ast_text_node` | Imprime nós e filhos com indentação por nível. |
| `print_ast_text` | Interface pública para a impressão textual indentada em `stdout`. |
| `dot_escape` | Escapa aspas e barras invertidas nos rótulos Graphviz. |
| `write_dot_label` | Produz o rótulo Graphviz específico para cada tipo de nó. |
| `write_dot_node` | Escreve nós, arestas de filhos e arestas tracejadas de irmãos no DOT. |
| `export_ast_dot` | Cria o arquivo DOT e informa sucesso/falha de escrita. |

`main.c` usa `print_tree` e `export_ast_dot`. `print_ast_text`, `printTree` e
`getHeight` também estão disponíveis, mas não são usados pelo fluxo principal.

## 6. Scanner — `scanner.lx`

O Flex lê o arquivo apontado por `yyin`. Cada regra reconhecida retorna um
token Bison; identificadores e números também preenchem o `yylval`. A contagem
de linha do Flex alimenta `yylloc`, usada pelo parser nos diagnósticos.

O estado exclusivo `COMMENT` ignora o conteúdo entre `/*` e `*/`. A linha de
início do comentário é guardada para que um comentário não terminado informe
onde começou. Espaços e quebras de linha fora de comentários são ignorados.
Um caractere que não corresponde a nenhuma regra chama `lexical_error`, que
imprime:

```text
ERRO LEXICO: <lexema>''
LINHA: <linha>
```

O erro léxico encerra a execução imediatamente com falha.

Funções e interface:

- `lexical_error(lexeme, line)`: formata e encerra após erro léxico.
- `yylex()`: função gerada pelo Flex; executa as regras e retorna tokens ao Bison.
- `token_name(token)`: converte tokens conhecidos em nomes legíveis.
- `YY_USER_ACTION`: macro aplicada pelo Flex para preencher a localização do token.

[`include/tokens.h`](../include/tokens.h) declara `yylex`, `yyin`, `yylineno` e
`token_name`.

## 7. Parser — `parser.y`

O Bison valida a sequência de tokens e constrói a AST nas ações semânticas das
produções. As listas de declarações, comandos, parâmetros e argumentos são
representadas com `append_sibling`. Cada produção cria o tipo de nó
correspondente e associa seus filhos em ordem.

O dangling-`else` é resolvido com as precedências `IF_WITHOUT_ELSE` e `ELSE`:
o `else` é associado ao `if` mais próximo. `%expect 1` registra o conflito
shift/reduce conhecido dessa gramática.

Há pontos de recuperação com o token especial `error`, incluindo declarações,
comandos e condições de `if`/`while`. O parser pode continuar para reportar
outros erros, mas `syntax_has_error` permanece ativo; portanto `main` não
executa análise semântica nem GCI para uma entrada com erro sintático.

Funções de diagnóstico:

- `copy_message_part(destination, capacity, start, length)`: copia uma parte
  limitada da mensagem do Bison para um buffer local.
- `yyerror(message)`: extrai token inesperado e esperado, converte `$end` em
  `EOF`, determina a linha e imprime
  `ERRO SINTATICO: token inesperado ''<token>'', esperado ''<token>'' LINHA: <X>`.

`ast_root` é a raiz global preenchida pela produção `program`;
`syntax_has_error` sinaliza ao `main` que ocorreu ao menos um erro.

## 8. Tabela de símbolos — `symtab.c`

`SymbolTable` contém um escopo global, o escopo atual e uma lista de todos os
escopos criados. Cada `Scope` aponta para o pai e armazena seus símbolos.
Escopos fechados deixam de ser o escopo atual, mas permanecem na lista para
serem impressos e destruídos ao final.

Um `Symbol` guarda nome, categoria, tipo, linha, tamanho de vetor e, para
funções, tipos e marcadores de parâmetros-vetor. As categorias são variável,
vetor, função, parâmetro escalar e parâmetro vetor. Uma declaração duplicada é
rejeitada apenas no escopo atual; `lookup` procura do escopo atual em direção
aos pais, permitindo shadowing em blocos internos.

### Funções de `symtab.c`

| Função | O que faz |
|---|---|
| `duplicate_string` | Cria uma cópia dinâmica de uma string ou retorna `NULL` se falhar. |
| `reserve_scopes` | Aumenta a capacidade da lista de escopos da tabela. |
| `reserve_symbols` | Aumenta a capacidade de símbolos do escopo informado. |
| `free_symbol` | Libera o nome e os metadados de parâmetros de um símbolo. |
| `create_scope` | Cria um escopo nomeado ligado ao escopo pai. |
| `symbol_table_init` | Zera a estrutura e cria o escopo global. |
| `symbol_table_destroy` | Libera símbolos, escopos e vetores dinâmicos da tabela. |
| `symbol_table_enter_scope` | Cria um escopo filho do atual e o torna atual. |
| `symbol_table_exit_scope` | Volta ao pai; não permite sair do escopo global. |
| `symbol_table_declare` | Copia e insere no escopo atual; retorna inserido, duplicado ou erro. |
| `symbol_table_lookup_current` | Busca apenas no escopo atual. |
| `symbol_table_lookup` | Busca no atual e depois nos pais, respeitando a declaração mais próxima. |
| `symbol_table_current_scope` | Retorna o escopo atual para consulta. |
| `type_name` | Converte o tipo de dado para texto. |
| `kind_name` | Converte a categoria do símbolo para texto em português. |
| `compare_symbol_pointers` | Ordena símbolos por nome para a apresentação. |
| `symbol_table_print` | Imprime todos os escopos, ordenando os nomes dentro de cada escopo. |

## 9. Análise semântica — `semantic.c`

`analyze_semantics` coordena a validação e retorna verdadeiro somente se não
houver erros. Primeiro declara `input`, `output` e todas as declarações
globais; isso permite resolver chamadas a funções declaradas mais adiante.
Depois verifica as regras de `main` e visita as funções.

O corpo externo de uma função compartilha o escopo da função com os parâmetros.
Um bloco composto aninhado cria um escopo filho nomeado, como
`main::block1`. Ao sair do bloco, a busca volta ao pai, mas o escopo continua
na tabela para impressão.

As expressões são resumidas por `ExpressionInfo`, com tipo, indicador de vetor,
indicador de l-value e validade. A análise verifica:

- símbolos declarados e uso correto de variável, vetor e função;
- duplicatas no mesmo escopo e tipos inválidos (`void` em variável/parâmetro);
- índices de vetor inteiros e escalares;
- operandos escalares inteiros em operadores e condições;
- destino atribuível e valor inteiro escalar em atribuições;
- número e tipo dos argumentos das funções;
- compatibilidade entre tipo da função e presença/valor do `return`;
- existência, posição, tipo e ausência de parâmetros de `main`.

### Funções principais e auxiliares

| Função | O que faz |
|---|---|
| `child` | Atalho para obter um filho da AST. |
| `report_error` | Marca o contexto inválido e imprime erro semântico com identificador e linha. |
| `report_internal_error` | Registra falha de memória e marca a análise inválida. |
| `create_symbol` | Constrói um `Symbol` temporário com dados de um nó AST. |
| `declare_symbol` | Encaminha a declaração à tabela e converte duplicata/falha em diagnóstico. |
| `is_array_kind` | Reconhece vetores e parâmetros-vetor. |
| `first_identifier` | Procura recursivamente o primeiro identificador de uma expressão. |
| `identifier_for` | Escolhe um identificador para diagnóstico ou usa a função atual. |
| `declare_builtins` | Insere as assinaturas de `input` e `output` no escopo global. |
| `declare_global` | Declara variável, vetor ou função global e copia a assinatura dos parâmetros. |
| `declare_globals` | Percorre a lista de declarações globais e as registra. |
| `validate_main` | Verifica existência, posição, tipo de retorno e ausência de parâmetros de `main`. |
| `declare_local` | Declara variável/vetor local e verifica tipo e tamanho do vetor. |
| `require_integer_scalar` | Confirma que uma expressão válida é escalar `int`. |
| `analyze_compound_contents` | Declara as variáveis do bloco antes de analisar os comandos. |
| `analyze_function` | Cria escopo da função, declara parâmetros, analisa corpo e retorna ao pai. |
| `analyze_return` | Compara o valor/ausência do retorno com o tipo de retorno da função. |
| `analyze_statement` | Analisa expressões, blocos, condições de `if`/`while` e retornos. |
| `invalid_expression` | Cria um resultado inválido para propagar erros sem interromper a visita. |
| `analyze_variable` | Resolve identificador e determina se é variável, parâmetro ou vetor. |
| `analyze_array_access` | Confirma que o nome é vetor e o índice é inteiro escalar. |
| `analyze_call` | Resolve função e verifica quantidade, tipo e natureza dos argumentos. |
| `analyze_expression` | Analisa constantes, variáveis, vetores, chamadas, operadores e atribuições. |
| `analyze_semantics` | Interface pública que coordena pré-declarações, `main` e visita das funções. |

Erros semânticos seguem o formato
`ERRO SEMANTICO: identificador '<id>' LINHA: <X>`. A análise acumula erros
sempre que possível e retorna falha ao `main`, que não trata a entrada como uma
compilação válida.

## 10. Geração de código intermediário — `gci.c`

A GCI é uma lista dinâmica de `Quad`, cada um com quatro strings:

```text
(operador, argumento1, argumento2, resultado)
```

O formato representa instruções de três endereços: uma operação normalmente
consome até dois argumentos e grava em um destino. Temporários são nomeados
`$t0`, `$t1`, ...; rótulos de controle de fluxo são `L0`, `L1`, ...
Identificadores aparecem com seus nomes fonte. A AST continua sendo a
representação em árvore; a lista de quádruplas é a sequência linear da fase
intermediária.

### Operadores emitidos

| Quadrupla | Significado |
|---|---|
| `FUN`, `ARG`, `END`, `HALT` | Início de função, parâmetro, fim de função e parada do programa. |
| `ALLOC`, `ALLOC_ARRAY` | Declaração de variável/vetor, com escopo e tamanho quando aplicável. |
| `+`, `-`, `*`, `/`, `<`, `<=`, `>`, `>=`, `==`, `!=` | Operação aritmética/comparação; resultado em temporário. |
| `=` | Atribuição escalar: `arg1` é o valor e `res` é o destino. |
| `[]` | Leitura: vetor em `arg1`, índice em `arg2`, resultado em `res`. |
| `[]=` | Escrita: vetor em `arg1`, índice em `arg2`, valor em `res`. |
| `PARAM`, `CALL` | Argumentos e chamada, incluindo quantidade e destino do retorno. |
| `IFF`, `GOTO`, `LAB` | Desvio quando falso, salto incondicional e definição de rótulo. |
| `RET` | Retorno com valor ou `-` para retorno sem expressão. |

Por exemplo, `result = a + b` é linearizado aproximadamente assim:

```text
(+, a, b, $t0)
(=, $t0, -, result)
```

A geração de `if` e `while` cria rótulos e desvios. Chamadas emitem quádruplas
de argumento antes da quadrupla `CALL`. As declarações das funções
predefinidas determinam se a chamada produz um temporário: `input` retorna
inteiro e `output` é `void`.

### Funções de `gci.c`

| Função | O que faz |
|---|---|
| `copy_string` | Cria cópia dinâmica de uma string usada pela GCI. |
| `free_quad` | Libera os quatro campos de uma quadrupla. |
| `clear_quads` | Libera a lista inteira e reinicializa a estrutura GCI. |
| `reserve_quads` | Aumenta a capacidade do vetor de quádruplas. |
| `emit` | Copia operador e operandos e acrescenta uma quadrupla ao vetor. |
| `new_temp` | Gera o próximo nome `$tN`. |
| `new_label` | Gera o próximo nome de rótulo `LN`. |
| `emit_label` | Acrescenta uma quadrupla `LAB`. |
| `function_returns_int` | Consulta AST e built-ins para decidir se a chamada retorna inteiro. |
| `generate_lvalue` | Converte variável ou elemento de vetor no destino de atribuição. |
| `clear_lvalue` | Libera o índice temporário de um destino-vetor. |
| `generate_call` | Gera argumentos, `PARAM`s e `CALL`; produz temporário para retorno inteiro. |
| `generate_expression` | Gera quádruplas para constantes, identificadores, vetores, operadores, atribuições e chamadas. |
| `generate_declaration` | Emite `ALLOC` ou `ALLOC_ARRAY` para uma declaração. |
| `generate_statement_list` | Visita comandos irmãos em ordem fonte. |
| `generate_compound` | Emite declarações locais e depois gera os comandos do bloco. |
| `generate_statement` | Gera quádruplas para `if`/`while`, expressões e `return`. |
| `generate_function` | Emite cabeçalho, parâmetros, corpo e finalização da função. |
| `generate_quads` | Interface pública: limpa a saída anterior e gera quádruplas para AST/programa. |
| `print_quads` | Imprime cada quadrupla no formato `(op, arg1, arg2, res)`. |

`GCI_INITIALIZER` inicializa a estrutura vazia. Depois do uso, chame
`clear_quads` para liberar as strings e o vetor. Se a geração falhar, as
quádruplas parciais são limpas e `generate_quads` retorna falso.

> **Não confundir as fases:** esta GCI ainda não é código da Tiny Machine. Nesta
> branch não há alocação de registradores/frames para TM, montagem de instruções,
> resolução de endereços numéricos nem execução das quádruplas.

## 11. Orquestração — `main.c`

`main(argc, argv)` é responsável pelo ciclo de vida da compilação:

1. Confere o argumento e abre o arquivo fonte.
2. Define `yyin`, chama `yyparse` e fecha o arquivo.
3. Interrompe se o parser falhou ou `syntax_has_error` foi definido.
4. Inicializa `SymbolTable` e chama `analyze_semantics`.
5. Imprime a tabela de símbolos.
6. Imprime a AST em árvore.
7. Inicializa `GCI`, chama `generate_quads` e imprime as quádruplas.
8. Libera a GCI, exporta `ast.dot`, destrói a tabela e libera a AST.

Cada caminho de erro tenta liberar os recursos já inicializados antes de
retornar `EXIT_FAILURE`.

## 12. Testes de exemplo

Os programas em [`tests/`](../tests/) são entradas manuais para as fases:

| Arquivo | Finalidade |
|---|---|
| `gcd.cm` | Programa válido com `input`, `output`, recursão, condição e aritmética. |
| `dangling_else.cm` | Confere a associação de `else` ao `if` mais próximo. |
| `escopos_semanticos.cm` | Exercita resolução e shadowing entre escopos. |
| `gci_arrays_calls.cm` | Exercita GCI com vetores, laços, condição e chamadas aninhadas. |
| `erro_lexico.cm` | Confere interrupção por caractere léxico inválido. |
| `erro_lexico_linha_comentario.cm` | Confere diagnóstico de comentário de bloco não terminado. |
| `erro_sintatico_recuperacao.cm` | Exercita recuperação sintática em entrada malformada. |
| `erro_sintatico_eof.cm` | Exercita diagnóstico de fim inesperado do arquivo. |
| `erro_semantico.cm` | Exercita rejeição de uso/declaração semanticamente inválidos. |
| `erro_semantico_main.cm` | Exercita as regras especiais de `main`. |

Exemplos válidos devem terminar com código zero. Arquivos de erro devem
terminar com falha e apresentar diagnóstico da fase correspondente.

## 13. Como seguir uma mudança no código

Para entender uma característica específica:

1. Encontre o padrão/token em [`src/scanner.lx`](../src/scanner.lx).
2. Localize a produção e a ação que cria os nós em [`src/parser.y`](../src/parser.y).
3. Consulte `NodeKind` e a organização dos filhos em
   [`include/ast.h`](../include/ast.h).
4. Veja como a AST é validada em [`src/semantic.c`](../src/semantic.c) e como
   o nome é resolvido em [`src/symtab.c`](../src/symtab.c).
5. Veja a tradução correspondente em [`src/gci.c`](../src/gci.c).
6. Confirme se [`src/main.c`](../src/main.c) chama a fase e acrescente um
   exemplo apropriado em `tests/`.

Por exemplo, para entender `vetor[i]`: o scanner produz os tokens;
`parser.y` cria `NODE_ARRAY_EXPR` com a expressão do índice; o semântico
confirma que `vetor` é vetor e `i` é escalar inteiro; a GCI gera `[]` para
leitura ou `[]=` para atribuição.
