# Compilador C- (C-Minus)

Projeto acadêmico de Compiladores da UNIFESP para implementação de um compilador da linguagem didática C-, baseada no Apêndice A de *Compiler Construction: Principles and Practice* (Kenneth C. Louden).

## Estado atual

Nesta primeira etapa, o projeto disponibiliza:

- Especificação léxica em Flex;
- Enumeração e nomes dos tokens;
- Ponto de entrada para execução do scanner;
- Arquivos de teste válidos e com erro léxico;
- Build para Windows com `mingw32-make`.

As etapas de parser, AST, tabela de símbolos, análise semântica e geração de código intermediário ainda serão implementadas.

## Estrutura

```text
C-Minus-Compiler/
|-- AGENTS.md
|-- Makefile
|-- README.md
|-- include/
|   `-- tokens.hpp
|-- src/
|   |-- main.cpp
|   |-- scanner.lx
|   `-- parser.y
|-- tests/
|   |-- gcd.cm
|   `-- erro_lexico.cm
`-- build/
```

## Requisitos

- Windows;
- MSYS2/MinGW-w64 com `g++ 16.2.0` ou compatível;
- Flex 2.5.4;
- `mingw32-make 4.4.1` ou compatível;
- C++17.

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

Exemplo de saída:

```text
Linha 1 | Token: INT | Lexema: int
Linha 1 | Token: ID | Lexema: gcd
```

Para testar a detecção de erro léxico:

```powershell
.\cmenos.exe .\tests\erro_lexico.cm
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