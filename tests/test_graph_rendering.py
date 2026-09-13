#!/usr/bin/env python3
"""Behavior checks for terminal geometry and domain gaps (Python 3)."""
from pathlib import Path
import subprocess
import tempfile
import atexit

TEMP = tempfile.TemporaryDirectory(prefix="mathscript-2d-tests-")
atexit.register(TEMP.cleanup)

BIN = Path(__file__).resolve().parents[1] / 'build' / 'mathscript'
WIDTH, HEIGHT = 200, 75
ZERO_ROW = (HEIGHT-1)//2
Y_LIMIT = 20*(HEIGHT-1)/(WIDTH-1)

def plot(expr):
    script = f'/tac off\n/calc\nx=7\ny=9\n/graph2d\n{expr}\n/calc\nx+y\n/exit\n'
    output = subprocess.run([str(BIN)], input=script, text=True,
                            capture_output=True, check=True, cwd=TEMP.name).stdout
    lines = output.splitlines()
    start = next(i for i, line in enumerate(lines) if line.endswith("200 x 75 canvas")) + 1
    rows = [line[12:] for line in lines[start:start+HEIGHT]]
    assert len(rows) == HEIGHT and all(len(row) == WIDTH for row in rows)
    assert lines[start+HEIGHT].startswith("            -10")
    assert lines[start+HEIGHT][12:].split()[:-1] == [str(n) for n in range(-10,11)]
    assert [int(line[:12]) for line in lines[start:start+HEIGHT] if line[:12].strip()] == list(range(7,-8,-1))
    assert 'Sampling: 32 points per column (6369 horizontal positions)' in output
    assert all(set(row) <= set(" .o") for row in rows), "extra border/grid marks"
    assert all(row[0] in " .o" and row[-1] in " .o" for row in rows), "side border"
    assert 'calc> 16\n' in output, 'graph changed stored variables'
    assert 'Grid:' not in output and '200 x 75 canvas' in output
    # Axes and curves now both use dots. Check geometry away from the axes.
    dots = [(c, r) for r, row in enumerate(rows) for c, char in enumerate(row)
            if char == '.' and r != ZERO_ROW and c != WIDTH//2]
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
out, dots = plot('y = 1')
assert len(dots) == WIDTH-1 and all(r == round((Y_LIMIT-1)/(2*Y_LIMIT)*(HEIGHT-1)) for c, r in dots)
out, dots = plot('y = 1000')
lines = out.splitlines()
start = next(i for i, line in enumerate(lines) if line.endswith("200 x 75 canvas")) + 1
axis = lines[start+ZERO_ROW]
assert axis[:12].strip() == '0'
assert all(c in '.o' for c in axis[12:]), 'horizontal axis must be continuous dots'
assert all(line[12+WIDTH//2] in '.o' for line in lines[start:start+HEIGHT]), 'vertical axis must be continuous dots'
assert sum(sum(c in '.o' for c in line[12:]) for line in lines[start:start+HEIGHT])==WIDTH+HEIGHT-1
assert all(set(line[12:]) <= {' ','.','o'} for line in lines[start:start+HEIGHT])
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
# More than four branches must survive at both positive and negative y.
out, dots = plot('(y-1)*(y+1)*(y-2)*(y+2)*(y-3)*(y+3)+0*x=0')
expected = {round((Y_LIMIT-y)/(2*Y_LIMIT)*(HEIGHT-1)) for y in [-3,-2,-1,1,2,3]}
assert {r for c,r in dots} == expected
# Scanning horizontally fills vertical components at off-grid x values.
out, dots = plot('(x-0.137)*y=0')
column = round((.137+10)/20*(WIDTH-1))
assert len({r for c,r in dots if c==column}) == HEIGHT-1
out, dots = plot('sqrt(x-2)+y=0')
assert dots and all(c>=round(.6*(WIDTH-1)) for c,r in dots)
out, dots = plot('1/(y-0.137)+0*x=0')
assert not dots, 'implicit pole was drawn as a root'
out, dots = plot('(y-0.137)^4+0*x=0')
assert dots and all(abs(Y_LIMIT-r*2*Y_LIMIT/(HEIGHT-1)-.137)<.11 for c,r in dots)
out, dots = plot('y=10000*(x-1)')
assert len({r for c,r in dots}) == HEIGHT-1, 'steep line has missing rows'
print('19 graph geometry/domain checks passed')
