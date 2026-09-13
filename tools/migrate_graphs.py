#!/usr/bin/env python3
"""Move legacy flat graph HTML into archives, without overwriting any graph."""
from pathlib import Path
import argparse
import os
import shutil

parser=argparse.ArgumentParser()
parser.add_argument('--output',type=Path,default=Path(__file__).resolve().parents[1]/'output')
args=parser.parse_args()
for dimension in (2,3):
    folder=args.output/f'graph{dimension}d'
    archive=folder/f'all{dimension}dgraphs'
    archive.mkdir(parents=True,exist_ok=True)
    moved=[]
    for source in sorted(args.output.glob(f'graph{dimension}d_*.html')):
        target=archive/source.name
        serial=1
        while target.exists():
            target=archive/f'{source.stem}_legacy_{serial}{source.suffix}'
            serial+=1
        shutil.move(str(source),str(target))
        moved.append(target)
    latest=folder/f'graph{dimension}d.html'
    if moved and not latest.exists():
        source=max(moved,key=lambda p:p.stat().st_mtime_ns)
        temporary=folder/f'.migration-{os.getpid()}.tmp'
        shutil.copy2(source,temporary)
        temporary.replace(latest)
    print(f'{dimension}D: archived {len(moved)} legacy graphs in {archive}')
