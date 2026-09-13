# Run from the project directory: build/mathscript examples/graph3d_examples.ms
/tac off
/graph3d
vccw
# Sphere: both hemispheres, with all three coordinates sampled.
x^2 + y^2 + z^2 = 25
# Switch to horizontal counterclockwise rotation.
hccw
# Saddle and trigonometric height surfaces.
z = x^2 - y^2
sin(x) + cos(y)
# Switch to vertical clockwise rotation.
vcw
# Vertical plane and cylinder; missing coordinates remain free.
x = 2
x^2 + y^2 = 9
# Switch to horizontal clockwise rotation.
hcw
# Double cone.
x^2 + y^2 = z^2
# Refine the view and sample density.
/range -6 6 -6 6 -6 6
/samples 96
x^2 + y^2 + z^2 = 25
exit
/exit
