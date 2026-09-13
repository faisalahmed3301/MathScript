#!/usr/bin/env python3
"""Compact exports, atomic latest updates, immutable archives, and migration."""
from pathlib import Path
import concurrent.futures
import re
import subprocess
import tempfile
import time

ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build/mathscript'
def run(folder,commands):
    result=subprocess.run([str(BIN)],input='/tac off\n'+commands+'\n/exit\n',text=True,capture_output=True,cwd=folder,timeout=5)
    assert result.returncode==0,result.stderr
    return result
with tempfile.TemporaryDirectory() as temporary:
    root=Path(temporary)
    for dimension,expr in [(3,'sin(x)*cos(y)+sin(y)*cos(z)+sin(z)*cos(x)=0'),(2,'x^2+y^2=25')]:
        start=time.perf_counter()
        commands=f'/graph{dimension}d\n'+('vccw\n' if dimension==3 else '')+expr
        out=run(root,commands);assert not out.stderr,out.stderr
        latest=root/f'output/graph{dimension}d/graph{dimension}d.html'
        archive=root/f'output/graph{dimension}d/all{dimension}dgraphs'
        files=list(archive.glob('*.html'));assert len(files)==1
        original=files[0].read_bytes();assert latest.read_bytes()==original
        assert len(original)<80000,'HTML must contain a model, not a massive point list'
        assert b'const points=' not in original
        out=run(root,commands);assert not out.stderr
        assert len(list(archive.glob('*.html')))==2;assert files[0].read_bytes()==original
        assert not list(latest.parent.glob('*.tmp'))
        print(f'{dimension}D compact export: {len(original):,} bytes, {(time.perf_counter()-start)/2:.3f}s average')
    # Every supported 2D form exports a usable model.
    for expression in ['y=sin(x)','y^2','x=y^3','3*x=1','a^2+b^2=9','y=5']:
        out=run(root,'/graph2d\n'+expression);assert 'Interactive 2D graph:' in out.stdout,out.stdout
    for dimension in [2,3]:
        latest=root/f'output/graph{dimension}d/graph{dimension}d.html';previous=latest.read_bytes()
        out=run(root,f'/graph{dimension}d\n'+('vccw\n' if dimension==3 else '')+'x+y+z+w=0')
        assert 'Semantic Error' in out.stdout;assert latest.read_bytes()==previous
    # Separate processes cannot clobber archive names; latest is one complete file.
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda i:run(root,f'/graph3d\nvccw\nz=x+{i}'),range(8)))
    archive=root/'output/graph3d/all3dgraphs';files=list(archive.glob('*.html'))
    assert len(files)==10
    assert (root/'output/graph3d/graph3d.html').read_bytes() in [p.read_bytes() for p in files]
    # Migration preserves colliding files and does not replace an existing latest.
    legacy=root/'output/graph3d_001.html';legacy.write_text('legacy graph')
    (archive/legacy.name).write_text('different archived graph')
    latest=root/'output/graph3d/graph3d.html';previous=latest.read_bytes()
    subprocess.run(['python3',str(ROOT/'tools/migrate_graphs.py'),'--output',str(root/'output')],check=True,capture_output=True)
    assert not legacy.exists();assert (archive/'graph3d_001.html').read_text()=='different archived graph'
    assert (archive/'graph3d_001_legacy_1.html').read_text()=='legacy graph';assert latest.read_bytes()==previous
with tempfile.TemporaryDirectory() as temporary:
    root=Path(temporary);folder=root/'output/graph3d';folder.mkdir(parents=True)
    (folder/'all3dgraphs').write_text('not a directory');(folder/'graph3d.html').write_text('previous graph')
    result=run(root,'/graph3d\nvccw\nz=x');assert result.stderr;assert (folder/'graph3d.html').read_text()=='previous graph'
print('Latest files, archive preservation, concurrent exports, invalid inputs, write failure and migration passed')
