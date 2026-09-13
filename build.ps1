# MathScript build script -- Windows PowerShell
#
# Requirements (install once):
#   Flex + Bison   -> https://github.com/lexxmark/winflexbison  (or via MSYS2/Chocolatey)
#   A C compiler   -> MinGW-w64 gcc (via MSYS2, or the "mingw" Chocolatey package)
#
# Usage (from the MathScript/ folder):
#   .\build.ps1
#   .\build\mathscript.exe
#
# If your Flex/Bison executables are named win_flex.exe / win_bison.exe
# (the WinFlexBison distribution), pass them in:
#   .\build.ps1 -Flex win_flex -Bison win_bison

param(
    [string]$Flex = "flex",
    [string]$Bison = "bison",
    [string]$Cc = "gcc"
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Force -Path build | Out-Null
New-Item -ItemType Directory -Force -Path output | Out-Null

Write-Host "== Step 1/3: Flex (lexical analyzer) ==" -ForegroundColor Cyan
& $Flex -o build/lex.yy.c src/lexer.l
if ($LASTEXITCODE -ne 0) { throw "flex failed" }

Write-Host "== Step 2/3: Bison (parser) ==" -ForegroundColor Cyan
& $Bison -d -o build/parser.tab.c src/parser.y
if ($LASTEXITCODE -ne 0) { throw "bison failed" }

Write-Host "== Step 3/3: GCC (compile + link) ==" -ForegroundColor Cyan
& $Cc -O2 -I build -I src `
    build/lex.yy.c `
    build/parser.tab.c `
    src/main.c src/ast.c src/symtab.c src/errors.c src/eval.c `
    src/ir.c src/util.c src/calc.c src/graph.c src/graph3d.c src/graph_export.c src/solver.c src/poly.c src/rootfind.c src/codegen.c `
    -o build/mathscript.exe -lm
if ($LASTEXITCODE -ne 0) { throw "gcc failed" }

Write-Host ""
Write-Host "Build complete: build\mathscript.exe" -ForegroundColor Green
Write-Host "Run it with:    .\build\mathscript.exe"
Write-Host "Or run a script: .\build\mathscript.exe examples\calc_examples.ms"
