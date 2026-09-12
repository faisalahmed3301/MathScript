#!/usr/bin/env bash
# MathScript regression tests -- macOS / Linux
# Run after building: ./build.sh && ./tests/run_tests.sh

set -uo pipefail
cd "$(dirname "$0")/.."

BIN=./build/mathscript
if [ ! -x "$BIN" ]; then
    echo "Build first: ./build.sh"
    exit 1
fi

pass=0
fail=0

# check "<label>" "<script>" "<expected substring>"
check() {
    local label="$1" script="$2" expect="$3"
    local out
    out=$(printf '%b' "$script" | "$BIN" 2>&1)
    if printf '%s' "$out" | grep -qF -e "$expect"; then
        echo "PASS: $label"
        pass=$((pass + 1))
    else
        echo "FAIL: $label (expected to find: $expect)"
        echo "--- actual output ---"
        printf '%s\n' "$out"
        echo "---------------------"
        fail=$((fail + 1))
    fi
}

# /calc
check "calc: 10 + 5"              "/calc\n10 + 5\nexit\n/exit\n"                  "15"
check "calc: precedence"          "/calc\n2 + 3 * 4\nexit\n/exit\n"               "14"
check "calc: parens"              "/calc\n(2 + 3) * 4\nexit\n/exit\n"             "20"
check "calc: sqrt+abs"            "/calc\nsqrt(25) + abs(-5)\nexit\n/exit\n"      "10"
check "calc: power assoc"         "/calc\n2^3^2\nexit\n/exit\n"                   "512"
check "calc: unary minus vs pow"  "/calc\n-2^2\nexit\n/exit\n"                    "-4"
check "calc: div by zero"         "/calc\n10 / 0\nexit\n/exit\n"                  "Math Error"
check "calc: unknown function"    "/calc\nfoo(5)\nexit\n/exit\n"                  "Semantic Error"
check "calc: lexical error"       "/calc\n10 @ 5\nexit\n/exit\n"                  "Lexical Error"
check "calc: syntax error"        "/calc\n10 + * 5\nexit\n/exit\n"                "Syntax Error"

# /graph
check "graph: parabola"           "/graph\ny = x^2\nexit\n/exit\n"                "Graph generated."
check "graph: needs y ="          "/graph\n2 + 2\nexit\n/exit\n"                  "Semantic Error"
check "graph: implicit mult y=5x" "/graph\ny=5x\nexit\n/exit\n"                   "t1 = 5 * x"
check "graph: bare eqn 3x=1"      "/graph\n3x=1\nexit\n/exit\n"                  "x = 0.333333"
check "graph: two-variable line"  "/graph\n3*x + 2*y = 6\nexit\n/exit\n"          "implicit relation"

# /eqn
check "eqn: linear"               "/eqn\n2*x + 5 = 15\nexit\n/exit\n"             "x = 5"
check "eqn: quadratic"            "/eqn\nx^2 - 5*x + 6 = 0\nexit\n/exit\n"        "x = 2"
check "eqn: quadratic root 2"     "/eqn\nx^2 - 5*x + 6 = 0\nexit\n/exit\n"        "x = 3"
check "eqn: difference of squares" "/eqn\nx^2 - 4 = 0\nexit\n/exit\n"             "x = -2"
check "eqn: cubic"                "/eqn\nx^3 - x = 0\nexit\n/exit\n"             "x = 0"
check "eqn: variable named y"     "/eqn\ny^2 = 4\nexit\n/exit\n"                  "y = 2"
check "eqn: implicit mult 3x=1"   "/eqn\n3x=1\nexit\n/exit\n"                    "x = 0.333333"

# power / implicit multiplication (previously reported as "power not working")
check "calc: pow with y"          "/calc\ny = 3\npow(y,2)\nexit\n/exit\n"        "9"
check "calc: implicit mult 2(3+4)" "/calc\n2(3+4)\nexit\n/exit\n"                "14"

# mode discipline
check "no mode active"            "2 + 2\n/exit\n"                                "No mode active"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
