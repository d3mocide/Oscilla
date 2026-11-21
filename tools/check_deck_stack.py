#!/usr/bin/env python3
"""Bound frames implicated in the USB LoRa-config stack-canary panic."""
import argparse
from pathlib import Path
import re
import subprocess
p=argparse.ArgumentParser()
p.add_argument('elf',nargs='?',type=Path,default=Path('firmware-cardputer/.pio/build/cardputer-adv/firmware.elf'))
p.add_argument('--objdump',default=str(Path.home()/'.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-objdump'))
a=p.parse_args()
output=subprocess.check_output([a.objdump,'-d','-C',str(a.elf)],text=True)
limits={'app::DeckApp::runDebugCommand(':2048,'app::DeckApp::debugSnapshot':1024,'app::DeckApp::sendLoraConfig(':1024,'loop()':2048,'ui::tickExternalGpsDisplay(':1024,'ui::drawExternalSystemStrip(':1024}
seen={};current=''
for line in output.splitlines():
 match=re.match(r'^[0-9a-f]+ <(.+)>:',line)
 if match:current=match[1]
 entry=re.search(r'\bentry\s+a1,\s+(0x[0-9a-f]+|\d+)',line)
 if not entry:continue
 for prefix,limit in limits.items():
  if (current==prefix if prefix=='loop()' else current.startswith(prefix)):
   frame=int(entry[1],0);seen[prefix]=max(seen.get(prefix,0),frame)
for prefix,frame in seen.items():
 print(f'{prefix}: {frame} bytes (limit {limits[prefix]})')
 if frame>limits[prefix]:raise SystemExit('FAIL: deck command stack frame exceeds panic-regression budget')
required=set(limits)
assert required<=seen.keys(), 'Missing required symbol/frame'
print('deck stack frames: bounded; call-chain/hardware qualification remains separate')
