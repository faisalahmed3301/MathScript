# MathScript example script -- /calc mode
# Run with:  build/mathscript examples/calc_examples.ms   (macOS/Linux)
#            build\mathscript.exe examples\calc_examples.ms  (Windows)

/calc

# Operator precedence
2 + 3 * 4
(10 + 5) * 2
(2 + 3)^2 - 4

# Power and unary minus precedence
-2^2
2^3^2

# Functions and constants
sqrt(25) + 2^3
abs(-5)
pow(2, 10)
sin(pi / 2)
log(e)

# Variables
x = 10
a = 5
result = x + a
result

# Errors (each is caught and reported, execution continues)
10 / 0
foo(5)
10 @ 5
10 + * 5

# Code generation demonstration: also compiles & runs equivalent C
gen (5 + 3) * 2

exit
/exit
