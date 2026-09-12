# MathScript example script -- /eqn mode
# Run with:  build/mathscript examples/eqn_examples.ms   (macOS/Linux)
#            build\mathscript.exe examples\eqn_examples.ms  (Windows)

/eqn

2*x + 5 = 15
x^2 - 4 = 0
x^2 - 5*x + 6 = 0
x^3 - x = 0

# implicit multiplication: "3x" means "3*x"
3x = 1

# any single variable name works, not just x
y^2 = 4

# Real and complex polynomial roots, including pow syntax
3*z + 6 = 0
y^2 - 2*y + 5 = 0
pow(z,3) - 1 = 0
(z-2)^3 = 0

exit
/exit
