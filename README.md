# Compilador C- (C-Minus)

Projeto acadêmico de Compiladores da UNIFESP para implementação de um compilador da linguagem didática C-, baseada no Apêndice A de *Compiler Construction: Principles and Practice* (Kenneth C. Louden).

## Estado atual

O projeto já implementa:

- Scanner em Flex, incluindo comentários de bloco e localização de erros léxicos;
- Parser em Bison com recuperação de erros sintáticos;
- Construção e impressão textual da AST, além da exportação opcional em Graphviz;
- Tabela de símbolos com escopos aninhados e análise semântica de declarações, usos, tipos, chamadas e retornos;
- Funções predefinidas `input()` e `output(int)`;
- Resolução do `else` associado ao `if` mais próximo;
- Build para Windows com `mingw32-make`.

Ainda falta a geração de código intermediário.

## Estrutura

```text
C-Minus-Compiler/
|-- AGENTS.md
|-- Makefile
|-- README.md
|-- include/
|   |-- ast.h
|   |-- semantic.h
|   |-- symtab.h
|   `-- tokens.h
|-- src/
|   |-- main.c
|   |-- scanner.lx
|   |-- parser.y
|   |-- ast.c
|   |-- semantic.c
|   `-- symtab.c
|-- tests/
|   |-- gcd.cm
|   |-- erro_lexico.cm
|   |-- erro_lexico_linha_comentario.cm
|   |-- dangling_else.cm
|   |-- erro_sintatico_recuperacao.cm
|   |-- erro_sintatico_eof.cm
|   |-- escopos_semanticos.cm
|   |-- erro_semantico.cm
|   `-- erro_semantico_main.cm
`-- build/
```

## Requisitos

- Windows;
- MSYS2/MinGW-w64 com `gcc` compatível com C11;
- Flex 2.5.4;
- `mingw32-make 4.4.1` ou compatível;
- C11.

No ambiente atual, o Flex configurado no Makefile está em:

```text
D:\Faculdade\Compiladores\GnuWin32\bin\flex.exe
```

Caso o Flex esteja em outro diretório, altere a variável `FLEX` no `Makefile`.

## Compilação

No diretório raiz do projeto:

```powershell
mingw32-make
```

Para remover o executável e os arquivos gerados:

```powershell
mingw32-make clean
```

## Execução

Execute o compilador informando um arquivo-fonte C-:

```powershell
.\cmenos.exe .\tests\gcd.cm
```

Para verificar o parser e a associação de `else`:

```powershell
.\cmenos.exe .\tests\dangling_else.cm
```

Para testar a detecção de erro léxico:

```powershell
.\cmenos.exe .\tests\erro_lexico.cm
.\cmenos.exe .\tests\erro_lexico_linha_comentario.cm
```

Para verificar a recuperação de erros sintáticos:

```powershell
.\cmenos.exe .\tests\erro_sintatico_recuperacao.cm
.\cmenos.exe .\tests\erro_sintatico_eof.cm
```

Para verificar escopos, chamadas de função e análise semântica:

```powershell
.\cmenos.exe .\tests\escopos_semanticos.cm
.\cmenos.exe .\tests\erro_semantico.cm
.\cmenos.exe .\tests\erro_semantico_main.cm
```

## Tokens reconhecidos

- Palavras-chave: `else`, `if`, `int`, `return`, `void`, `while`;
- Identificadores: letras;
- Números inteiros: dígitos;
- Operadores: `+`, `-`, `*`, `/`, `<`, `<=`, `>`, `>=`, `==`, `!=`, `=`;
- Delimitadores: `;`, `,`, `(`, `)`, `[`, `]`, `{`, `}`;
- Comentários de bloco: `/* ... */`.

O scanner interrompe a compilação ao encontrar um caractere inválido e apresenta a mensagem:

```text
ERRO LEXICO: <lexema>''
LINHA: <linha>
```