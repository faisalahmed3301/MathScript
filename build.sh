#!/usr/bin/env bash
# MathScript build script -- macOS / Linux
#
# Requirements (install once):
#   macOS:  xcode-select --install        # gives you a C compiler (clang, aliased as cc)
#           brew install flex bison       # macOS ships very old versions otherwise
#   Linux:  sudo apt install flex bison gcc
#
# Usage (from the MathScript/ directory):
#   chmod +x build.sh
#   ./build.sh
#   ./build/mathscript

set -euo pipefail

CC=${CC:-cc}

# Homebrew's flex/bison are "keg-only" (kept out of PATH) so they don't
# shadow macOS's ancient system copies; pick them up automatically if present.
if [ -d "/opt/homebrew/opt/flex/bin" ]; then PATH="/opt/homebrew/opt/flex/bin:$PATH"; fi
if [ -d "/opt/homebrew/opt/bison/bin" ]; then PATH="/opt/homebrew/opt/bison/bin:$PATH"; fi
if [ -d "/usr/local/opt/flex/bin" ]; then PATH="/usr/local/opt/flex/bin:$PATH"; fi
if [ -d "/usr/local/opt/bison/bin" ]; then PATH="/usr/local/opt/bison/bin:$PATH"; fi
export PATH

mkdir -p build output

echo "== Step 1/3: Flex (lexical analyzer) =="
flex -o build/lex.yy.c src/lexer.l

echo "== Step 2/3: Bison (parser) =="
bison -d -o build/parser.tab.c src/parser.y

echo "== Step 3/3: $CC (compile + link) =="
"$CC" -I build -I src \
    build/lex.yy.c \
    build/parser.tab.c \
    src/main.c src/ast.c src/symtab.c src/errors.c src/eval.c \
    src/ir.c src/util.c src/calc.c src/graph.c src/solver.c src/codegen.c \
    -o build/mathscript -lm

echo
echo "Build complete: build/mathscript"
echo "Run it with:     ./build/mathscript"
echo "Or run a script: ./build/mathscript examples/calc_examples.ms"
