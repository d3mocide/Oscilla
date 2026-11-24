#!/usr/bin/env python3
"""Render real companion source at panel resolution with synthetic models."""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path('/tmp/oscilla-viewport-review'))
    parser.add_argument('--sync-guide-assets', action='store_true',
                        help='Copy canonical 320x240 Observatory v2 PNGs into docs/brand/assets/external/')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    fonts = ROOT / 'firmware-cardputer/.pio/libdeps/cardputer-adv/M5GFX/src'
    if not fonts.exists():
        fonts = ROOT / 'firmware-cardputer/.pio/libdeps/cardputer-adv/M5GFX@0.2.28/src'
    models = ['spectrum_model','scan_model','bt_model','contacts_model','zig_model',
              'lora_model','ook_model','lora_sweep_model','lora_framing','deauth_model','anti_surveillance_model','gnss_model','session_review',
              'baseline_watch','signal_hunt']
    views = ['spectrum','wifi','bt','contacts','lora','zig','drive','system','gnss','hunt','ook']
    sources = [ROOT/'tools/viewport_fixtures.cpp']
    sources += [ROOT/f'firmware-cardputer/src/model/{m}.cpp' for m in models]
    sources += [ROOT/f'firmware-cardputer/src/ui/external_{v}_view.cpp' for v in views]
    sources += [ROOT/'firmware-cardputer/src/ui/observatory_fonts.cpp',ROOT/'firmware-cardputer/src/ui/external_card_views.cpp',ROOT/'firmware-cardputer/src/ui/external_contact_detail_view.cpp']
    sources += [ROOT/f'firmware-cardputer/src/ocp/{m}.cpp' for m in ['ocp_parser','ocp_csv']]
    sources += [ROOT/'protocol/ocp_text.c',ROOT/'tools/viewport_deck.cpp']
    sources += [ROOT/f'firmware-cardputer/src/ui/{v}.cpp' for v in ['chrome','help_view','contacts_view','contact_detail_view','wifi_detail_view','bt_detail_view','sweep_view','bt_view','deauth_view','zig_view','anti_surveillance_view','subghz_view','ook_view','gnss_fix_view','spectrum_view','session_view','lora_detail_view','settings_view','signal_hunt_view']]
    with tempfile.TemporaryDirectory(prefix='oscilla-view-render-') as name:
        binary = Path(name)/'render'
        subprocess.run(['g++','-std=c++17','-O1','-Wall','-Wextra',
                        '-I'+str(ROOT/'tools/viewport_stub'),'-I'+str(ROOT/'tools/hoststub'),
                        '-I'+str(fonts),'-I'+str(ROOT/'firmware-cardputer/src'),
                        '-I'+str(ROOT/'protocol'),*[str(p) for p in sources],'-o',str(binary)],check=True)
        subprocess.run([str(binary),str(output)],check=True)
    def write_png(path,width,height,rgb):
        def chunk(kind,data):
            return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
        rows=b''.join(b'\0'+rgb[y*width*3:(y+1)*width*3] for y in range(height))
        data=b'\x89PNG\r\n\x1a\n'
        data+=chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))
        data+=chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')
        path.write_bytes(data)
    images={}
    for ppm in output.glob('*.ppm'):
        header,rgb=ppm.read_bytes().split(b'\n255\n',1)
        width,height=map(int,header.splitlines()[1].split())
        assert len(rgb)==width*height*3
        if (width,height)==(320,240):images[ppm.stem]=rgb
        write_png(ppm.with_suffix('.png'),width,height,rgb)
    source=(fonts/'lgfx/Fonts/glcdfont.h').read_text()
    start=source.index('{',source.index('font[]'))+1
    table=source[start:source.index('};',start)]
    table=re.sub(r'//[^\n]*|/\*.*?\*/','',table,flags=re.S)
    glyphs=bytes(int(value,16) for value in re.findall(r'0x([0-9a-fA-F]{2})',table))
    assert len(glyphs)==1280
    names = ['packet-monitor','channel-field','wifi','bluetooth','contacts','requests',
             'lora','pans','nodes','drive','gnss','deauth','correlation','link','health','ap-detail','settings']
    for mode in ['active','empty','cached']:
        width,height=1360,1350
        sheet=bytearray(bytes((11,9,13))*(width*height))
        for i,name in enumerate(names):
            x,y=(i%4)*340,(i//4)*270
            for n,char in enumerate(name+' / SYNTHETIC'):
                for column in range(5):
                    bits=glyphs[ord(char)*5+column]
                    for row in range(8):
                        if bits & (1<<row):
                            offset=((y+4+row)*width+x+10+n*6+column)*3
                            sheet[offset:offset+3]=bytes((244,236,217))
            rgb=images[f'{name}-{mode}']
            for row in range(240):
                offset=((y+22+row)*width+x+10)*3
                sheet[offset:offset+960]=rgb[row*960:(row+1)*960]
        write_png(output/f'contact-sheet-{mode}.png',width,height,sheet)
    subprocess.run(["python3", str(ROOT/"tools/test_session_highlight.py"),
                    str(output/"sessions-catalog-active.ppm")],check=True)
    if args.sync_guide_assets:
        guide_dir = ROOT / 'docs/brand/assets/external'
        guide_dir.mkdir(parents=True, exist_ok=True)
        guide_views = [
            'wifi', 'packet-monitor', 'channel-field', 'lora', 'drive', 'gnss',
            'bluetooth', 'contacts', 'pans', 'deauth', 'health', 'sessions-summary',
        ]
        copied = 0
        for name in guide_views:
            for mode in ('active', 'cached', 'empty'):
                src_png = output / f'{name}-{mode}.png'
                if src_png.exists():
                    shutil.copy2(src_png, guide_dir / src_png.name)
                    copied += 1
        deck_dir = ROOT / 'docs/brand/assets/deck'
        deck_dir.mkdir(parents=True, exist_ok=True)
        deck_views = [
            'deck-ap-radio', 'deck-clients', 'deck-advertiser',
            'deck-client-detail', 'deck-sessions-catalog', 'deck-sessions-summary',
        ]
        for name in deck_views:
            src_png = output / f'{name}-active.png'
            if src_png.exists():
                shutil.copy2(src_png, deck_dir / src_png.name)
                copied += 1
        print(f'Synced {copied} guide assets to docs/brand/assets/{{external,deck}}')
    print(f'Converted {len(list(output.glob("*.ppm")))} source-render fixtures in {output}')

if __name__ == '__main__':
    main()
