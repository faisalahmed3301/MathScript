#!/usr/bin/env bash
# MathScript regression tests -- macOS / Linux
# Run after building: ./build.sh && ./tests/run_tests.sh

set -uo pipefail
cd "$(dirname "$0")/.."

BIN="$PWD/build/mathscript"
if [ ! -x "$BIN" ]; then
    echo "Build first: ./build.sh"
    exit 1
fi

test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
cd "$test_dir"

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

# /graph2d
check "graph: parabola"           "/graph2d\ny = x^2\nexit\n/exit\n"                "Graph generated."
check "graph: needs y ="          "/graph2d\n2 + 2\nexit\n/exit\n"                  "Semantic Error"
check "graph: implicit mult y=5x" "/graph2d\ny=5x\nexit\n/exit\n"                   "t1 = 5 * x"
check "graph: bare eqn 3x=1"      "/graph2d\n3x=1\nexit\n/exit\n"                  "x = 0.333333"
check "graph: two-variable line"  "/graph2d\n3*x + 2*y = 6\nexit\n/exit\n"          "implicit relation"

# Sideways curves: verify computed coordinates, including both branches.
check "graph: bare y squared lower branch" "/graph2d\ny^2\nexit\n/exit\n" "(4, -2)"
check "graph: bare y squared upper branch" "/graph2d\ny^2\nexit\n/exit\n" "(4, 2)"
check "graph: bare y cubed negative" "/graph2d\ny^3\nexit\n/exit\n" "(-8, -2)"
check "graph: bare y cubed positive" "/graph2d\ny^3\nexit\n/exit\n" "(8, 2)"
check "graph: y squared equation" "/graph2d\ny^2 = x\nexit\n/exit\n" "(4, -2)"
check "graph: y cubed equation" "/graph2d\ny^3 = x\nexit\n/exit\n" "(-8, -2)"
check "graph: reversed cubic equation" "/graph2d\nx = y^3\nexit\n/exit\n" "(8, 2)"
check "graph: y on both sides lower branch" "/graph2d\ny = y^3 + x\nexit\n/exit\n" "(0, -1)"
check "graph: y on both sides upper branch" "/graph2d\ny = y^3 + x\nexit\n/exit\n" "(0, 1)"

# Function-call power syntax must retain both branches and negative roots.
check "graph: pow square lower branch" "/graph2d\npow(y,2)\nexit\n/exit\n" "(4, -2)"
check "graph: pow square upper branch" "/graph2d\npow(y,2)\nexit\n/exit\n" "(4, 2)"
check "graph: pow cube negative" "/graph2d\npow(y,3)\nexit\n/exit\n" "(-8, -2)"
check "graph: pow cube positive" "/graph2d\npow(y,3)\nexit\n/exit\n" "(8, 2)"
check "graph: pow square equation" "/graph2d\npow(y,2) = x\nexit\n/exit\n" "(4, -2)"
check "graph: pow cube equation" "/graph2d\npow(y,3) = x\nexit\n/exit\n" "(-8, -2)"
check "graph: pow reversed square" "/graph2d\nx = pow(y,2)\nexit\n/exit\n" "(4, 2)"
check "graph: pow reversed cube" "/graph2d\nx = pow(y,3)\nexit\n/exit\n" "(8, 2)"

# Check actual canvas dimensions and markers, excluding labels and indentation.
check_grid() {
    local expr="$1" out
    out=$(printf '/tac off\n/graph2d\n%s\n/exit\n' "$expr" | "$BIN" 2>&1)
    if printf '%s\n' "$out" | awk '
        /200 x 75 canvas$/ && !started { plotting = 1; started = 1; next }
        plotting && rows < 75 {
            row = substr($0, 13, 200)
            if (length($0) != 212 || row ~ /[^ .o*]/) bad = 1
            if (index(row, ".")) dots = 1
            rows++; next
        }
        plotting && rows == 75 { if ($0 !~ /^            -10/) bad = 1; plotting = 0 }
        END { exit !(rows == 75 && dots && !bad && !plotting) }
    '; then
        echo "PASS: graph: 200x75 dot canvas ($expr)"
        pass=$((pass + 1))
    else
        echo "FAIL: graph: 200x75 dot canvas ($expr)"
        fail=$((fail + 1))
    fi
}
check_grid "y = x^2"
check_grid "pow(y,2)"
check_grid "pow(y,3)"

# /eqn
check "eqn: linear"               "/eqn\n2*x + 5 = 15\nexit\n/exit\n"             "x = 5"
check "eqn: quadratic"            "/eqn\nx^2 - 5*x + 6 = 0\nexit\n/exit\n"        "x = 2"
check "eqn: quadratic root 2"     "/eqn\nx^2 - 5*x + 6 = 0\nexit\n/exit\n"        "x = 3"
check "eqn: difference of squares" "/eqn\nx^2 - 4 = 0\nexit\n/exit\n"             "x = -2"
check "eqn: cubic"                "/eqn\nx^3 - x = 0\nexit\n/exit\n"             "x = 0"
check "eqn: variable named y"     "/eqn\ny^2 = 4\nexit\n/exit\n"                  "y = 2"
check "eqn: implicit mult 3x=1"   "/eqn\n3x=1\nexit\n/exit\n"                    "x = 0.333333"

# Polynomial roots: arbitrary variables and complex conjugates.
check "eqn: linear z" "/eqn\n3*z + 6 = 0\n/exit\n" "z = -2"
check "eqn: quadratic y complex plus" "/eqn\ny^2 - 2*y + 5 = 0\n/exit\n" "y = 1 + 2i"
check "eqn: quadratic y complex minus" "/eqn\ny^2 - 2*y + 5 = 0\n/exit\n" "y = 1 - 2i"
check "eqn: pow z imaginary" "/eqn\npow(z,2) + 1 = 0\n/exit\n" "z = 0 + 1i"
check "eqn: cubic z real" "/eqn\npow(z,3) - 1 = 0\n/exit\n" "z = 1"
check "eqn: cubic z complex plus" "/eqn\npow(z,3) - 1 = 0\n/exit\n" "z = -0.5 + 0.866025i"
check "eqn: cubic z complex minus" "/eqn\npow(z,3) - 1 = 0\n/exit\n" "z = -0.5 - 0.866025i"
check "eqn: cubic outside scan" "/eqn\n(y-30)*(y-40)*(y-50) = 0\n/exit\n" "y = 50"
check "eqn: cubic repeated" "/eqn\n(z-2)^3 = 0\n/exit\n" "z = 2"
check "eqn: both equation sides" "/eqn\nz^3 = 6*z^2 - 11*z + 6\n/exit\n" "z = 3"
check "eqn: identity" "/eqn\ny-y = 0\n/exit\n" "Infinitely many solutions."
check "eqn: inconsistent" "/eqn\nz-z = 1\n/exit\n" "No solution."

# power / implicit multiplication (previously reported as "power not working")
check "calc: pow with y"          "/calc\ny = 3\npow(y,2)\nexit\n/exit\n"        "9"
check "calc: implicit mult 2(3+4)" "/calc\n2(3+4)\nexit\n/exit\n"                "14"

# mode discipline
check "no mode active"            "2 + 2\n/exit\n"                                "No mode active"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
