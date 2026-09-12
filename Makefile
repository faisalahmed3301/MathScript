# MathScript Makefile -- macOS / Linux (uses `make`, same steps as build.sh)
# On Windows, use build.ps1 instead (PowerShell doesn't run Makefiles by default).

CC      ?= cc
FLEX    ?= flex
BISON   ?= bison
CFLAGS  = -I build -I src -Wall
LIBS    = -lm

SRCS = src/main.c src/ast.c src/symtab.c src/errors.c src/eval.c \
       src/ir.c src/util.c src/calc.c src/graph.c src/solver.c src/rootfind.c src/codegen.c

.PHONY: all clean run

all: build/mathscript

build/lex.yy.c: src/lexer.l | build
	$(FLEX) -o build/lex.yy.c src/lexer.l

build/parser.tab.c: src/parser.y | build
	$(BISON) -d -o build/parser.tab.c src/parser.y

build/mathscript: build/lex.yy.c build/parser.tab.c $(SRCS) | output
	$(CC) $(CFLAGS) build/lex.yy.c build/parser.tab.c $(SRCS) -o build/mathscript $(LIBS)

build:
	mkdir -p build

output:
	mkdir -p output

run: all
	./build/mathscript

clean:
	rm -rf build/*.c build/*.h build/mathscript output/generated.c output/generated
