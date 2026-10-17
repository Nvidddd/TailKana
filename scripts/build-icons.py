#!/usr/bin/env python3
"""Render TailKana's original vector mark as macOS app and template menu icons."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
assets=root/'assets';icons=root/'work/TailKana.iconset';icons.mkdir(exist_ok=True)
for size in (16,32,128,256,512):
 for scale in (1,2):
  name=f'icon_{size}x{size}'+('@2x' if scale==2 else '')+'.png'
  subprocess.run(['rsvg-convert','-w',str(size*scale),'-h',str(size*scale),'-o',str(icons/name),str(assets/'tailkana.svg')],check=True)
subprocess.run(['iconutil','-c','icns',str(icons),'-o',str(assets/'TailKana.icns')],check=True)
subprocess.run(['rsvg-convert','-w','1024','-h','1024','-o',str(assets/'tailkana.png'),str(assets/'tailkana.svg')],check=True)
subprocess.run(['rsvg-convert','-f','pdf','-o',str(assets/'tailkana-menu.pdf'),str(assets/'tailkana-menu.svg')],check=True)
print(assets/'tailkana.png')
