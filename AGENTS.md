# AGENTS.md — Compilador C- (C-Minus)

Este documento define o contexto do projeto, os requisitos da especificação acadêmica e as restrições técnicas do ambiente de desenvolvimento para orientar colaboradores e assistentes de código neste repositório.

---

## 1. Visão Geral do Projeto

- **Disciplina:** Compiladores — UNIFESP
- **Docente:** Prof. Dr. Rodrigo Colnago Contreras
- **Objetivo:** Implementar um compilador funcional em **C** para a linguagem didática **C-**, seguindo rigorosamente a especificação do **Apêndice A** do livro *Compiler Construction: Principles and Practice* (Kenneth C. Louden, 1997).
- **Diretório do Projeto:** `D:\Faculdade\Compiladores\C-Minus-Compiler`
- **Restrições Acadêmicas:**
  - Todo o código-fonte deve ser de autoria própria do grupo (o esqueleto de arquivos de Louden serve apenas como referência didática).
  - O projeto deve ser limpo, modular e bem comentado, pois todos os integrantes passarão por arguição individual sobre qualquer módulo do compilador.

---

## 2. Ambiente de Desenvolvimento e Restrições de Ferramentas

O desenvolvimento ocorre em **Windows** utilizando **PowerShell** e ferramentas instaladas via **MSYS2 (MinGW-w64)** e **GnuWin32**.

| Ferramenta | Caminho / Origem | Versão Instalada | Observações Críticas de Compatibilidade |
| :--- | :--- | :--- | :--- |
| **Sistema / Shell** | Windows (PowerShell) | — | Caminhos usam `\` no Windows; evitar comandos exclusivos de bash/POSIX nos scripts de build. |
| **Compilador C** | MSYS2 (`x86_64-w64-mingw32`) | GCC | Compilar em C11 (`-std=c11`). |
| **Scanner** | `D:\Faculdade\Compiladores\GnuWin32\bin\flex.exe` | `flex 2.5.4` | **Versão legada (2.5.4).** Ver regras de compatibilidade abaixo. |
| **Parser** | `D:\Faculdade\Compiladores\GnuWin32\bin\bison.exe` | `bison 2.4.1` | **Versão legada (2.4.1).** Ver regras de compatibilidade abaixo. |
| **Automação Build** | `mingw32-make` (`x86_64-w64-mingw32`) | `GNU Make 4.4.1` | Invocar sempre via `mingw32-make`. |

### ⚠️ Diretrizes Obrigatórias para Flex 2.5.4 e Bison 2.4.1 (GnuWin32)

Como as versões do Flex e do Bison instaladas via GnuWin32 são anteriores às séries 2.6+ e 3.x+, **não utilize diretivas modernas** que causam erro de sintaxe nesses geradores:

1. **Regras para o Flex (`2.5.4`):**
   - **NÃO usar** `%option never-interactive` (inexistente na 2.5.4). Em vez disso, definir `#define YY_NEVER_INTERACTIVE 1` dentro do bloco `%{ ... %}` do `.lx`.
   - **NÃO usar** `%option nounput` ou `%option noinput` caso gerem aviso de opção desconhecida; se necessário, suprimir via pragmas ou definições `#define YY_NO_INPUT`.
   - Usar `%option noyywrap` e `%option yylineno` (suportados na 2.5.4).
   - Definir `#define YY_NO_UNISTD_H` se houver conflito com cabeçalhos POSIX no Windows.

2. **Regras para o Bison (`2.4.1`):**
   - **NÃO usar** diretivas do Bison 3.x como `%define api.value.type` ou `%define parse.error verbose`.
   - Utilizar a abordagem clássica compatível com C: `%union { ... }`, `%error-verbose`, e geração de cabeçalho via flag `-d` (`bison -d -o build/parser.tab.c src/parser.y`).
   - Declarar `extern int yylex(void);` e `void yyerror(const char* s);` explicitamente no `.y`.

---

## 3. Arquitetura e Módulos Exigidos

O compilador deve operar em pipeline sequencial e estruturar-se nos seguintes módulos:

```text
C-Minus-Compiler/
├── include/            # Cabeçalhos (.h) de AST, Tabela de Símbolos, Semântico, GCI e TM
├── src/
│   ├── main.c          # Ponto de entrada, leitura do arquivo .cm e orquestração das fases
│   ├── scanner.lx      # Especificação léxica (Flex 2.5.4)
│   ├── parser.y        # Gramática C-, recuperação de erros e construção da AST (Bison 2.4.1)
│   ├── ast.c           # Estruturas de nós da AST, impressão textual e/ou exportação .dot
│   ├── symtab.c        # Tabela de Símbolos (hash table + pilha de escopos ou equivalente)
│   ├── semantic.c      # Análise semântica sobre a AST
│   ├── gci.c           # Geração de código intermediário em quádruplas
│   └── tm_codegen.c    # Tradução das quádruplas para Tiny Machine
├── tests/              # Programas de teste em C- (casos válidos e casos de erro)
├── build/              # Arquivos intermediários gerados (.c/.h do Flex/Bison e objetos)
├── Makefile            # Script de compilação para mingw32-make
├── README.md           # Instruções de compilação e execução
└── AGENTS.md           # Este arquivo de contexto
```

### Especificação por Fase

1. **Scanner (Análise Léxica):**
   - Reconhecer palavras-chave (`else`, `if`, `int`, `return`, `void`, `while`), identificadores (`[a-zA-Z]+`), números (`[0-9]+`), operadores (`+`, `-`, `*`, `/`, `<`, `<=`, `>`, `>=`, `==`, `!=`, `=`) e delimitadores (`;`, `,`, `(`, `)`, `[`, `]`, `{`, `}`).
   - Ignorar espaços em branco e comentários de bloco `/* ... */` (sem aninhamento), mantendo contagem precisa de linhas (`yylineno`).
   - **Comportamento em erro:** Interromper imediatamente a compilação (`exit(EXIT_FAILURE)`).

2. **Parser (Análise Sintática) & AST:**
   - Implementar a gramática livre de contexto da linguagem C- (Louden, Apêndice A), tratando o conflito *dangling-else*.
   - Construir a Árvore Sintática Abstrata (AST) durante o reconhecimento.
   - **Comportamento em erro:** Exibir mensagem padronizada e aplicar recuperação de erro (`error` token no Bison) para continuar a análise sempre que possível.

3. **Tabela de Símbolos:**
   - Armazenar no mínimo: **nome do identificador**, **tipo** (`int` ou `void`, além de distinção entre variável simples, vetor e função) e **escopo** (global ou nome da função/bloco onde foi declarado).

4. **Analisador Semântico:**
   - Percorrer a AST validando:
     - Uso de variáveis ou funções não declaradas;
     - Declarações duplicadas no mesmo escopo;
     - Incompatibilidades de tipo em expressões, atribuições, retornos e passagem de parâmetros;
     - Regras da função `main()` (deve existir, ser a última declaração do programa, ter retorno `void` e parâmetros `void`);
     - Regras das funções pré-definidas `input()` (retorna `int`, sem argumentos) e `output(x)` (retorna `void`, recebe 1 argumento `int`).

5. **Gerador de Código Intermediário (3 Endereços):**
   - Linearizar a AST gerando código de três endereços **sem otimizações** (ex.: `t1 = a + b`, `t2 = t1 * c`, `output t2`, rótulos e desvios condicionais/incondicionais para `if` e `while`).

6. **Backend Tiny Machine (Louden):**
   - Emitir instruções TM numeradas, usando os formatos RR (`ADD r,s,t`) e RM (`LD r,d(s)`, `JLT r,d(7)`).
   - Reservar `r5` para GP, `r6` para FP e `r7` para PC; iniciar a memória de dados zerada, com `dMem[0]` igual ao endereço máximo.
   - Usar offsets positivos a partir de GP para globais e frames descendentes a partir de FP, preservando FP e PC de retorno em `-1(FP)` e `-2(FP)`.
   - Suportar recursão, parâmetros-array, arrays locais/globais, chamadas, I/O e desvios; manter a GCI como representação intermediária anterior à TM.

---

## 4. Formatos Obrigatórios de Mensagens de Erro

A formatação exata das mensagens de erro é critério explícito de avaliação (**1,0 ponto**). Respeite literalmente o padrão abaixo:

- **Erro Léxico** (aborta a compilação imediatamente):
  ```text
  ERRO LEXICO: <lexema>''
  LINHA: <X>
  ```

- **Erro Sintático** (exibe e tenta recuperação):
  ```text
  ERRO SINTATICO: token inesperado ''<token>'', esperado ''<token>'' LINHA: <X>
  ```

- **Erro Semântico**:
  ```text
  ERRO SEMANTICO: identificador '<id>' LINHA: <X>
  ```

---

## 5. Saídas Obrigatórias ao Final da Compilação

Para programas sem erros impeditivos, o compilador deve imprimir ao final da execução:
1. **Tabela de Símbolos completa** (identificadores, tipos e escopos);
2. **AST** em formato textual indentado (e opcionalmente exportação `.dot` para Graphviz);
3. **Código Intermediário** em três endereços.

---

## 6. Critérios de Avaliação e Entregáveis

| Critério | Peso |
| :--- | :---: |
| Implementação correta do scanner (léxico) | 2,0 |
| Implementação correta do parser (sintático) & AST | 2,0 |
| Implementação correta do analisador semântico | 2,0 |
| Mensagens de erro (correção e clareza) | 1,0 |
| Estrutura/clareza do código & Tabela de Símbolos | 1,0 |
| Apresentação e domínio do projeto | 2,0 |
| **Total** | **10,0** |

**Entregáveis Finais:** Código-fonte completo em C, `Makefile`, `README.md`, exemplos de teste em C- e relatório técnico detalhando o projeto e a participação individual de cada integrante.