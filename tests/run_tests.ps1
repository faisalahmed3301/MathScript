# MathScript regression tests -- Windows PowerShell
# Run after building: .\build.ps1 ; .\tests\run_tests.ps1
#
# Each case is written to a temporary .ms script file and run via
# "mathscript.exe <file>" (script mode) rather than piped through
# the pipeline -- PowerShell 5.1 mangles multi-line strings piped
# to a native executable's stdin, so a real file is used instead.

$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$Bin = "build\mathscript.exe"
if (-not (Test-Path $Bin)) {
    Write-Host "Build first: .\build.ps1" -ForegroundColor Red
    exit 1
}

$pass = 0
$fail = 0
$tmp = Join-Path $env:TEMP "mathscript_test_case.ms"

function Check($label, $lines, $expect) {
    Set-Content -Path $tmp -Value $lines -Encoding ASCII
    $out = (& $Bin $tmp | Out-String)
    if ($out -like "*$expect*") {
        Write-Host "PASS: $label" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "FAIL: $label (expected to find: $expect)" -ForegroundColor Red
        Write-Host "--- actual output ---"
        Write-Host $out
        Write-Host "---------------------"
        $script:fail++
    }
}

# /calc
Check "calc: 10 + 5"             @("/calc", "10 + 5", "exit", "/exit")             "15"
Check "calc: precedence"         @("/calc", "2 + 3 * 4", "exit", "/exit")          "14"
Check "calc: parens"             @("/calc", "(2 + 3) * 4", "exit", "/exit")        "20"
Check "calc: sqrt+abs"           @("/calc", "sqrt(25) + abs(-5)", "exit", "/exit") "10"
Check "calc: power assoc"        @("/calc", "2^3^2", "exit", "/exit")              "512"
Check "calc: unary minus vs pow" @("/calc", "-2^2", "exit", "/exit")               "-4"
Check "calc: div by zero"        @("/calc", "10 / 0", "exit", "/exit")             "Math Error"
Check "calc: unknown function"   @("/calc", "foo(5)", "exit", "/exit")             "Semantic Error"
Check "calc: lexical error"      @("/calc", "10 @ 5", "exit", "/exit")             "Lexical Error"
Check "calc: syntax error"       @("/calc", "10 + * 5", "exit", "/exit")           "Syntax Error"

# /graph
Check "graph: parabola"          @("/graph", "y = x^2", "exit", "/exit")           "Graph generated."
Check "graph: needs y ="         @("/graph", "2 + 2", "exit", "/exit")             "Semantic Error"

# /eqn
Check "eqn: linear"                @("/eqn", "2*x + 5 = 15", "exit", "/exit")      "x = 5"
Check "eqn: quadratic root 2"      @("/eqn", "x^2 - 5*x + 6 = 0", "exit", "/exit") "x = 2"
Check "eqn: quadratic root 3"      @("/eqn", "x^2 - 5*x + 6 = 0", "exit", "/exit") "x = 3"
Check "eqn: difference of squares" @("/eqn", "x^2 - 4 = 0", "exit", "/exit")       "x = -2"
Check "eqn: cubic"                 @("/eqn", "x^3 - x = 0", "exit", "/exit")       "x = 0"

# mode discipline
Check "no mode active"           @("2 + 2", "/exit")                              "No mode active"

Remove-Item -Path $tmp -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "$pass passed, $fail failed"
if ($fail -ne 0) { exit 1 }
