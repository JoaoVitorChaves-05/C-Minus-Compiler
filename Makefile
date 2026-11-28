CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude -Ibuild
FLEX = flex
BISON = bison

TARGET = cmenos.exe
SRC_DIR = src
BUILD_DIR = build

YACC_SRC = $(SRC_DIR)/parser.y
YACC_GEN_CPP = $(BUILD_DIR)/parser.tab.cpp
YACC_GEN_HPP = $(BUILD_DIR)/parser.tab.hpp

LEX_SRC = $(SRC_DIR)/scanner.lx
LEX_GEN = $(BUILD_DIR)/lex.yy.cpp

CPP_SRCS = $(SRC_DIR)/main.cpp $(SRC_DIR)/ast.cpp

all: $(TARGET)

$(BUILD_DIR):
	if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)

$(YACC_GEN_CPP) $(YACC_GEN_HPP): $(YACC_SRC) | $(BUILD_DIR)
	$(BISON) -d -o $(YACC_GEN_CPP) $(YACC_SRC)

$(LEX_GEN): $(LEX_SRC) $(YACC_GEN_HPP) | $(BUILD_DIR)
	$(FLEX) -t $(LEX_SRC) > $(LEX_GEN)

$(TARGET): $(YACC_GEN_CPP) $(LEX_GEN) $(CPP_SRCS)
	$(CXX) $(CXXFLAGS) $(YACC_GEN_CPP) $(LEX_GEN) $(CPP_SRCS) -o $(TARGET)

clean:
	if exist $(BUILD_DIR) rmdir /S /Q $(BUILD_DIR)
	if exist $(TARGET) del /Q $(TARGET)
	if exist ast.dot del /Q ast.dot