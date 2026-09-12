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
Check "graph: implicit mult y=5x" @("/graph", "y=5x", "exit", "/exit")             "t1 = 5 * x"
Check "graph: bare eqn 3x=1"     @("/graph", "3x=1", "exit", "/exit")              "x = 0.333333"
Check "graph: two-variable line" @("/graph", "3*x + 2*y = 6", "exit", "/exit")     "implicit relation"

# Sideways curves: verify computed coordinates, including both branches.
Check "graph: bare y squared lower branch" @("/graph", "y^2", "exit", "/exit") "(4, -2)"
Check "graph: bare y squared upper branch" @("/graph", "y^2", "exit", "/exit") "(4, 2)"
Check "graph: bare y cubed negative" @("/graph", "y^3", "exit", "/exit") "(-8, -2)"
Check "graph: bare y cubed positive" @("/graph", "y^3", "exit", "/exit") "(8, 2)"
Check "graph: y squared equation" @("/graph", "y^2 = x", "exit", "/exit") "(4, -2)"
Check "graph: y cubed equation" @("/graph", "y^3 = x", "exit", "/exit") "(-8, -2)"
Check "graph: reversed cubic equation" @("/graph", "x = y^3", "exit", "/exit") "(8, 2)"
Check "graph: y on both sides lower branch" @("/graph", "y = y^3 + x", "exit", "/exit") "(0, -1)"
Check "graph: y on both sides upper branch" @("/graph", "y = y^3 + x", "exit", "/exit") "(0, 1)"

# Function-call power syntax must retain both branches and negative roots.
Check "graph: pow square lower branch" @("/graph", "pow(y,2)", "exit", "/exit") "(4, -2)"
Check "graph: pow square upper branch" @("/graph", "pow(y,2)", "exit", "/exit") "(4, 2)"
Check "graph: pow cube negative" @("/graph", "pow(y,3)", "exit", "/exit") "(-8, -2)"
Check "graph: pow cube positive" @("/graph", "pow(y,3)", "exit", "/exit") "(8, 2)"
Check "graph: pow square equation" @("/graph", "pow(y,2) = x", "exit", "/exit") "(4, -2)"
Check "graph: pow cube equation" @("/graph", "pow(y,3) = x", "exit", "/exit") "(-8, -2)"
Check "graph: pow reversed square" @("/graph", "x = pow(y,2)", "exit", "/exit") "(4, 2)"
Check "graph: pow reversed cube" @("/graph", "x = pow(y,3)", "exit", "/exit") "(8, 2)"

# Check actual canvas dimensions and markers, excluding labels and indentation.
function Check-Grid($expr) {
    Set-Content -Path $tmp -Value @("/tac off", "/graph", $expr, "/exit") -Encoding ASCII
    $lines = @(& $Bin $tmp)
    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '200 x 75 canvas$') { $start = $i + 1; break }
    }
    $valid = $start -ge 0 -and $lines.Count -gt ($start + 75)
    $dots = $false
    if ($valid) {
        for ($i = $start; $i -lt ($start + 75); $i++) {
            if ($lines[$i] -notmatch '^.{12}[ .|+\-]{200}$') { $valid = $false }
            if ($lines[$i].Contains('.')) { $dots = $true }
        }
        if ($lines[$start + 75] -notmatch '^            -10') { $valid = $false }
    }
    if ($valid -and $dots) {
        Write-Host "PASS: graph: 200x75 dot canvas ($expr)" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "FAIL: graph: 200x75 dot canvas ($expr)" -ForegroundColor Red
        $script:fail++
    }
}
Check-Grid "y = x^2"
Check-Grid "pow(y,2)"
Check-Grid "pow(y,3)"

# /eqn
Check "eqn: linear"                @("/eqn", "2*x + 5 = 15", "exit", "/exit")      "x = 5"
Check "eqn: quadratic root 2"      @("/eqn", "x^2 - 5*x + 6 = 0", "exit", "/exit") "x = 2"
Check "eqn: quadratic root 3"      @("/eqn", "x^2 - 5*x + 6 = 0", "exit", "/exit") "x = 3"
Check "eqn: difference of squares" @("/eqn", "x^2 - 4 = 0", "exit", "/exit")       "x = -2"
Check "eqn: cubic"                 @("/eqn", "x^3 - x = 0", "exit", "/exit")       "x = 0"
Check "eqn: variable named y"      @("/eqn", "y^2 = 4", "exit", "/exit")           "y = 2"
Check "eqn: implicit mult 3x=1"    @("/eqn", "3x=1", "exit", "/exit")              "x = 0.333333"

# Polynomial roots: arbitrary variables and complex conjugates.
Check "eqn: linear z" @("/eqn", "3*z + 6 = 0", "/exit") "z = -2"
Check "eqn: quadratic y complex plus" @("/eqn", "y^2 - 2*y + 5 = 0", "/exit") "y = 1 + 2i"
Check "eqn: quadratic y complex minus" @("/eqn", "y^2 - 2*y + 5 = 0", "/exit") "y = 1 - 2i"
Check "eqn: pow z imaginary" @("/eqn", "pow(z,2) + 1 = 0", "/exit") "z = 0 + 1i"
Check "eqn: cubic z real" @("/eqn", "pow(z,3) - 1 = 0", "/exit") "z = 1"
Check "eqn: cubic z complex plus" @("/eqn", "pow(z,3) - 1 = 0", "/exit") "z = -0.5 + 0.866025i"
Check "eqn: cubic z complex minus" @("/eqn", "pow(z,3) - 1 = 0", "/exit") "z = -0.5 - 0.866025i"
Check "eqn: cubic outside scan" @("/eqn", "(y-30)*(y-40)*(y-50) = 0", "/exit") "y = 50"
Check "eqn: cubic repeated" @("/eqn", "(z-2)^3 = 0", "/exit") "z = 2"
Check "eqn: both equation sides" @("/eqn", "z^3 = 6*z^2 - 11*z + 6", "/exit") "z = 3"
Check "eqn: identity" @("/eqn", "y-y = 0", "/exit") "Infinitely many solutions."
Check "eqn: inconsistent" @("/eqn", "z-z = 1", "/exit") "No solution."

# power / implicit multiplication (previously reported as "power not working")
Check "calc: pow with y"          @("/calc", "y = 3", "pow(y,2)", "exit", "/exit") "9"
Check "calc: implicit mult 2(3+4)" @("/calc", "2(3+4)", "exit", "/exit")           "14"

# mode discipline
Check "no mode active"           @("2 + 2", "/exit")                              "No mode active"

Remove-Item -Path $tmp -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "$pass passed, $fail failed"
if ($fail -ne 0) { exit 1 }
