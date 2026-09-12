# MathScript example script -- /graph mode
# Run with:  build/mathscript examples/graph_examples.ms   (macOS/Linux)
#            build\mathscript.exe examples\graph_examples.ms  (Windows)

/graph

y = x^2
y = 2*x + 3
y = x^2 - 4*x + 3
y = sin(x)
y = sqrt(x)

# implicit multiplication also works: "5x" means "5*x"
y = 5x

# bare equations (no "y = ") are accepted too:
# one free variable -> solved and shown as point(s) on a number line
3*x = 1
# two free variables -> plotted as an implicit curve (line, circle, ...)
3*x + 2*y = 6
x^2 + y^2 = 25

# Functions of y: bare expressions mean x = f(y)
y^2
y^3
y^2 = x
y^3 = x

# Equivalent function-call notation
pow(y,2)
pow(y,3)
pow(y,2) = x
x = pow(y,3)

exit
/exit
