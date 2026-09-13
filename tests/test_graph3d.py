#!/usr/bin/env python3
"""Verify sampled geometry, all branches, domains, modes and settings."""
import json
import math
from pathlib import Path
import re
import subprocess
import tempfile

BIN = Path(__file__).resolve().parents[1] / 'build' / 'mathscript'
checks = 0

def run(script, cwd):
    result = subprocess.run([str(BIN)], input=script+'\n/exit\n', text=True,
                            capture_output=True, check=True, cwd=cwd, timeout=60)
    assert not result.stderr, result.stderr
    return result.stdout

with tempfile.TemporaryDirectory() as directory:
    cwd = Path(directory)
    (cwd/'output').mkdir()
    graph_files=lambda:set((cwd/'output/graph3d/all3dgraphs').glob('*.html'))

    def surface(expr, bounds='-5 5 -5 5 -5 5', samples=24):
        global checks
        output = run('/tac off\n/calc\nx=7\ny=9\nz=11\n/graph3d\nvccw\n'
                     f'/range {bounds}\n/samples {samples}\n{expr}\n/calc\nx+y+z', cwd)
        assert 'calc> 27\n' in output, 'plot changed stored variables'
        assert '[GRAPH3D mode activated]' in output
        assert '3D graph generated' in output, output
        path = re.search(r'Interactive 3D graph: (\S+)', output).group(1)
        html = (cwd/path).read_text()
        points = json.loads(subprocess.check_output(['node',str(BIN.parents[1]/'tests/sample_export.js'),str(cwd/path)],text=True,timeout=10))
        assert len(html.encode())<80000, 'export should store the equation, not a point cloud'
        assert 'https://' not in html and 'http://' not in html, 'viewer must work offline'
        assert all(all(math.isfinite(v) for v in p) for p in points)
        checks += 1
        return points, output

    points, _ = surface('x^2+y^2+z^2=16')
    assert len(points)>1000
    assert all(abs(x*x+y*y+z*z-16)<2e-5 for x,y,z in points)
    assert {(x>0,y>0,z>0) for x,y,z in points if x*y*z} == {
        (x,y,z) for x in (False,True) for y in (False,True) for z in (False,True)}

    points, _ = surface('z=x^2-y^2')
    assert points and all(abs(z-x*x+y*y)<2e-5 for x,y,z in points)
    points, _ = surface('x=0.137')
    assert points and all(abs(x-.137)<3e-5 for x,y,z in points)
    assert min(y for x,y,z in points)==-5 and max(z for x,y,z in points)==5
    points, _ = surface('x^2+y^2=9')
    assert points and all(abs(x*x+y*y-9)<2e-5 for x,y,z in points)
    assert {min(p[2] for p in points),max(p[2] for p in points)}=={-5,5}
    points, _ = surface('(z-1)*(z+1)*(z-2)*(z+2)*(z-3)*(z+3)+0*x+0*y=0')
    assert {round(z) for x,y,z in points}=={-3,-2,-1,1,2,3}
    assert all(abs(z-round(z))<3e-5 for x,y,z in points)
    points, _ = surface('sin(x)+cos(y)')
    assert points and all(abs(z-math.sin(x)-math.cos(y))<2e-5 for x,y,z in points)
    points, _ = surface('sqrt(x-2)+y=z')
    assert points and all(x>=2 and abs(math.sqrt(x-2)+y-z)<2e-5 for x,y,z in points)
    points, _ = surface('z=log(x)+y')
    assert points and all(x>0 and abs(z-math.log(x)-y)<3e-5 for x,y,z in points)
    points, _ = surface('1/(z-0.137)+0*x+0*y=0')
    assert not points, 'a pole is not a zero'
    points, output = surface('x^2+y^2+z^2=-1')
    assert not points
    points, _ = surface('(z-0.137)^4+0*x+0*y=0')
    assert points and all(abs(z-.137)<3e-5 for x,y,z in points), 'tangent sheet lost'
    points, _ = surface('z=x*(x-1)*(x+1)*(x-2)+y')
    assert points and all(abs(z-x*(x-1)*(x+1)*(x-2)-y)<3e-5 for x,y,z in points), 'quartic misclassified as quadratic'
    points, _ = surface('(x^2+y^2+z^2+5)^2=36*(x^2+y^2)')
    assert points and all(abs((x*x+y*y+z*z+5)**2-36*(x*x+y*y))<5e-4 for x,y,z in points)
    assert any(z>1 for x,y,z in points) and any(z<-1 for x,y,z in points)
    points, _ = surface('a^2+b^2+c^2=16')
    assert points and all(abs(a*a+b*b+c*c-16)<2e-5 for a,b,c in points)
    points, _ = surface('(x-20)^2+(y-20)^2+(z-20)^2=4', '17 23 17 23 17 23')
    assert points and all(abs((x-20)**2+(y-20)**2+(z-20)**2-4)<3e-5 for x,y,z in points)
    coarse, _ = surface('x^2+y^2+z^2=16', samples=8)
    fine, _ = surface('x^2+y^2+z^2=16', samples=32)
    assert len(fine)>len(coarse)*4, 'resolution did not improve coverage'

    before = graph_files()
    out=run('/graph3d\nvccw\nx+y+z+w=0\nz=bad(x)\nz=sqrt(x,y)\nexit\n/graph2d\nx+y+z=0\n/graph\nexit', cwd)
    assert out.count('Semantic Error')==4, out
    assert graph_files()==before, 'invalid equations exported a graph'
    assert '[Leaving GRAPH3D mode]' in out and '[GRAPH2D mode activated]' in out
    out=run('/graph3d\nvccw\n/range 1 0 -1 1 -1 1\n/range -1 1 -1 1\n/range -1 1 -1 1 -1 1 junk\n/samples 0\n/samples 1.5\n/samples 513', cwd)
    assert 'finite and increasing' in out and out.count('Usage: /range')==2 and out.count('Usage: /samples')==3
    out=run('/tac off\n/graph3d\nvccw\n/range 17 23 17 23 17 23\n/graph2d\ny=x',cwd)
    assert 'Plot (x from -10 to 10, y from -7.43719 to 7.43719)' in out
    out=run('/tac off\n/graph2d\n/range 30 40 -1 1\nsin(x)=0\n/range 1 2 3 4 junk',cwd)
    assert 'x = 31.4159' in out and 'x = 37.6991' in out and 'Usage: /range' in out
    out=run('/tac off\n/graph2d\nsin(10*x)=0',cwd)
    assert 'x = 9.73894' in out, 'number-line roots still capped at 32'
    checks += 5

    # A fresh 3D entry requires a choice; settings/help/mode exits still work.
    before=graph_files()
    out=run('/graph3d\nx^2+y^2+z^2=1\nwrong\n/range -2 2 -2 2 -2 2\n/samples 8\n/help\n/graph2d\ny=0',cwd)
    assert 'rotation> ' in out and out.count('Choose a rotation first:')==2
    assert graph_files()==before
    assert 'Graph generated.' in out and '3D sampling set to 8' in out
    # Choices persist across equations, but reset on re-entering 3D.
    out=run('/tac off\n/graph3d\nhcw\n/samples 4\nz=x\nz=y\n/graph3d\nz=x\nexit\n/calc\nhcw=7\nhcw+1',cwd)
    assert out.count('Rotation: hcw (x axis, clockwise)')==2
    assert 'Choose a rotation first:' in out and 'calc> 8' in out
    # Rotation commands cannot silently change 2D mode or its variables.
    out=run('/tac off\n/calc\nvccw=3\n/graph2d\ny=vccw\n/calc\nvccw',cwd)
    assert 'Rotation set:' not in out and 'calc> 3' in out
    checks+=3

print(f'{checks} 3D geometry/domain/mode checks passed')
