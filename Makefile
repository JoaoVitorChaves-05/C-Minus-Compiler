CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
FLEX = D:\Faculdade\Compiladores\GnuWin32\bin\flex.exe

TARGET = cmenos.exe
SRC_DIR = src
BUILD_DIR = build

LEX_SRC = $(SRC_DIR)/scanner.lx
LEX_GEN = $(BUILD_DIR)/lex.yy.cpp
MAIN_SRC = $(SRC_DIR)/main.cpp

all: $(TARGET)

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(LEX_GEN): $(LEX_SRC) | $(BUILD_DIR)
	$(FLEX) $(LEX_SRC)
	move /Y lex.yy.c $(LEX_GEN)

$(TARGET): $(LEX_GEN) $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) $(LEX_GEN) $(MAIN_SRC) -o $(TARGET)

clean:
	del /Q $(BUILD_DIR)\* $(TARGET) 2>nul || rm -rf $(BUILD_DIR) $(TARGET)