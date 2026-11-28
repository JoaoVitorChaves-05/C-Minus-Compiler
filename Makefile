CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Iinclude -Ibuild
FLEX = flex
BISON = bison

TARGET = cmenos.exe
SRC_DIR = src
BUILD_DIR = build

YACC_SRC = $(SRC_DIR)/parser.y
YACC_GEN_C = $(BUILD_DIR)/parser.tab.c
YACC_GEN_H = $(BUILD_DIR)/parser.tab.h

LEX_SRC = $(SRC_DIR)/scanner.lx
LEX_GEN_C = $(BUILD_DIR)/lex.yy.c

C_SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/ast.c $(SRC_DIR)/symtab.c $(SRC_DIR)/semantic.c

all: $(TARGET)

$(BUILD_DIR):
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

$(YACC_GEN_C) $(YACC_GEN_H): $(YACC_SRC) | $(BUILD_DIR)
	$(BISON) -d -o $(YACC_GEN_C) $(YACC_SRC)

$(LEX_GEN_C): $(LEX_SRC) $(YACC_GEN_H) | $(BUILD_DIR)
	$(FLEX) -t $(LEX_SRC) > $(LEX_GEN_C)

$(TARGET): $(YACC_GEN_C) $(LEX_GEN_C) $(C_SRCS) $(wildcard include/*.h)
	$(CC) $(CFLAGS) $(YACC_GEN_C) $(LEX_GEN_C) $(C_SRCS) -o $(TARGET)

clean:
	if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
	if exist $(TARGET) del /Q $(TARGET)
	if exist ast.dot del /Q ast.dot