#!/usr/bin/env python3
"""Behavior checks for terminal geometry and domain gaps (Python 3)."""
from pathlib import Path
import subprocess

BIN = Path(__file__).resolve().parents[1] / 'build' / 'mathscript'
WIDTH, HEIGHT = 200, 75
ZERO_ROW = (HEIGHT-1)//2
Y_LIMIT = 20*(HEIGHT-1)/(WIDTH-1)

def plot(expr):
    script = f'/tac off\n/calc\nx=7\ny=9\n/graph\n{expr}\n/calc\nx+y\n/exit\n'
    output = subprocess.run([str(BIN)], input=script, text=True,
                            capture_output=True, check=True).stdout
    lines = output.splitlines()
    start = next(i for i, line in enumerate(lines) if line.endswith("200 x 75 canvas")) + 1
    rows = [line[12:] for line in lines[start:start+HEIGHT]]
    assert len(rows) == HEIGHT and all(len(row) == WIDTH for row in rows)
    assert lines[start+HEIGHT].startswith("            -10")
    assert all(set(row) <= set(" .|+-") for row in rows), "extra border/grid marks"
    assert all(row[0] in " .-" and row[-1] in " .-" for row in rows), "side border"
    assert 'calc> 16\n' in output, 'graph changed stored variables'
    assert 'Grid:' not in output and '200 x 75 canvas' in output
    dots = [(c, r) for r, row in enumerate(rows) for c, char in enumerate(row) if char == '.']
    return output, dots

out, dots = plot('x^2+y^2=25')
assert dots and len(dots) > 100
# A dot represents a rounded cell, so allow one cell of geometric error.
for c, r in dots:
    x, y = -10+c*20/(WIDTH-1), Y_LIMIT-r*2*Y_LIMIT/(HEIGHT-1)
    assert abs((x*x+y*y)**.5 - 5) < .3
width = max(c for c, r in dots)-min(c for c, r in dots)
height = max(r for c, r in dots)-min(r for c, r in dots)
# In 2:1 terminal cells, twice as many columns as rows is a round circle.
assert abs(width/(2*height) - 1) < .05, 'circle is physically stretched' 

out, dots = plot('y = x^2')
assert len(dots) > 100, 'dense samples should fill steep curves'
assert 'clipped' in out
assert all(r <= ZERO_ROW for c, r in dots), 'parabola cannot have negative y'

out, dots = plot('y = 1/x')
assert dots
assert not any(WIDTH//2-1 <= c <= WIDTH//2 and ZERO_ROW-3 <= r <= ZERO_ROW+3 for c, r in dots), 'false bridge across pole'

out, dots = plot('y = sqrt(x-2)')
assert dots and all(c >= round(.6*(WIDTH-1)) for c, r in dots), 'undefined region must stay empty'
assert 'Semantic Error' not in out and 'Math Error' not in out

out, dots = plot('y = pow(-1,0.5)')
assert not dots and 'No real curve points in this view.' in out

out, dots = plot('y = 1000')
assert not dots and 'clipped' in out, 'offscreen values must not stick to border'

for expr in ['pow(y,2)', 'pow(y,3)']:
    out, dots = plot(expr)
    assert dots and any(r < ZERO_ROW for c, r in dots) and any(r > ZERO_ROW for c, r in dots)

out, dots = plot('a^2+z^2=25')
assert 'Plot (a from -10 to 10, z from -7.43719 to 7.43719)' in out
# An odd canvas height must put y=0 on the same row as the x-axis.
out, dots = plot('y = 0')
assert len(dots) == WIDTH and all(r == ZERO_ROW for c, r in dots)
out, dots = plot('y = 1000')
lines = out.splitlines()
start = next(i for i, line in enumerate(lines) if line.endswith("200 x 75 canvas")) + 1
axis = lines[start+ZERO_ROW]
assert axis[:12].strip() == '0'
assert axis[12:].count('-') == WIDTH-1 and axis[12+WIDTH//2] == '+'
# The photographed sideways parabola must keep both branches and use
# significantly more of the height than the old [-20, 20] viewport.
out, dots = plot('pow(y,2)=4*x+3')
assert min(r for c,r in dots) < 8 and max(r for c,r in dots) > HEIGHT-9
for c,r in dots:
    x = -10+c*20/(WIDTH-1)
    y = Y_LIMIT-r*2*Y_LIMIT/(HEIGHT-1)
    assert abs(y*y-4*x-3) < 1.6, 'dot does not lie near the equation'
out, dots = plot('y=x')
for c,r in dots:
    assert abs((-10+c*20/(WIDTH-1))-(Y_LIMIT-r*2*Y_LIMIT/(HEIGHT-1))) < .16
print('13 graph geometry/domain checks passed')
