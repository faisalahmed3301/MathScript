#!/usr/bin/env python3
"""Check complete root sets, including multiplicity and conjugate pairs.
Run after building: python3 tests/test_polynomial_roots.py
"""
import itertools
from pathlib import Path
import re
import subprocess

BIN = Path(__file__).resolve().parents[1] / 'build' / 'mathscript'
CASES = [
    ('3*z+6=0', 'z', [-2]),
    ('y^2-2*y+5=0', 'y', [1-2j, 1+2j]),
    ('pow(z,2)+1=0', 'z', [-1j, 1j]),
    ('pow(z,3)-1=0', 'z', [1, -.5-.866025403784j, -.5+.866025403784j]),
    ('y^3+8=0', 'y', [-2, 1-1.732050807569j, 1+1.732050807569j]),
    ('z^3=6*z^2-11*z+6', 'z', [1, 2, 3]),
    ('(y-30)*(y-40)*(y-50)=0', 'y', [30, 40, 50]),
    ('(z-2)^3=0', 'z', [2, 2, 2]),
    ('(y-2)^2*(y+1)=0', 'y', [-1, 2, 2]),
    ('z^3=0', 'z', [0, 0, 0]),
    ('y^2=0', 'y', [0, 0]),
    ('(z^3-z)/2=0', 'z', [-1, 0, 1]),
    ('sqrt(4)*y^2-8=0', 'y', [-2, 2]),
    ('0.000001*z^3-0.000001=0', 'z', [1, -.5-.866025403784j, -.5+.866025403784j]),
]
# Many independently known real-root cubics, including mixed signs.
for roots in itertools.combinations(range(-4, 5), 3):
    expr = '*'.join(f'(z-({r}))' for r in roots) + '=0'
    CASES.append((expr, 'z', list(roots)))

for expr, var, expected in CASES:
    # Existing values must neither fix the unknown nor be changed by solving.
    script = f'/calc\n{var}=123\n/eqn\n/tac off\n{expr}\n/calc\n{var}\n/exit\n'
    result = subprocess.run([str(BIN)], input=script, text=True, capture_output=True, check=True)
    output = result.stdout
    equation_output = output.split('[EQN mode activated]', 1)[1].split('[CALC mode activated]', 1)[0]
    actual = [complex(value.replace(' ', '').replace('i', 'j'))
              for value in re.findall(rf'^{var} = (.+)$', equation_output, re.MULTILINE)]
    assert len(actual) == len(expected), (expr, actual, output)
    assert any(all(abs(a-b) <= 6e-6 * max(1, abs(b)) for a, b in zip(actual, perm))
               for perm in itertools.permutations(expected)), (expr, actual, expected)
    assert 'calc> 123\n' in output, (expr, 'variable value was changed', output)
print(f'{len(CASES)} complete polynomial root-set checks passed')
