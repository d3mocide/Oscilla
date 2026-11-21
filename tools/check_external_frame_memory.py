"""Run the actual external strip dispatcher against synthetic host models."""
from pathlib import Path
import argparse
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--dispatcher', type=Path)
    args = parser.parse_args()
    fonts = ROOT / 'firmware-cardputer/.pio/libdeps/cardputer-adv/M5GFX/src'
    if not fonts.exists():
        fonts = ROOT / 'firmware-cardputer/.pio/libdeps/cardputer-adv/M5GFX@0.2.28/src'
    models = ['spectrum_model', 'scan_model', 'bt_model', 'contacts_model', 'zig_model',
              'lora_model', 'lora_framing', 'deauth_model', 'anti_surveillance_model',
              'gnss_model', 'session_review']
    views = ['spectrum', 'wifi', 'bt', 'contacts', 'lora', 'zig', 'drive', 'system', 'gnss']
    sources = [ROOT/'firmware-cardputer/test/host/external_frame_memory_test.cpp']
    sources += [ROOT/f'firmware-cardputer/src/model/{m}.cpp' for m in models]
    sources += [ROOT/f'firmware-cardputer/src/ui/external_{v}_view.cpp' for v in views]
    sources += [ROOT/f'firmware-cardputer/src/ui/{v}.cpp' for v in (
        'observatory_fonts', 'external_card_views', 'external_contact_detail_view', 'wifi_detail_view',
        'bt_detail_view', 'session_view', 'lora_detail_view', 'chrome')]
    sources += [ROOT/f'firmware-cardputer/src/ocp/{m}.cpp' for m in ('ocp_parser', 'ocp_csv')]
    sources += [ROOT/'protocol/ocp_text.c', ROOT/'tools/viewport_deck.cpp']
    with tempfile.TemporaryDirectory(prefix='oscilla-frame-memory-') as name:
        binary = Path(name)/'test'
        mutation = ([f'-DOSCILLA_FRAME_DISPATCHER="{args.dispatcher.resolve()}"']
                    if args.dispatcher else [])
        subprocess.run(['g++', '-std=c++17', '-O1', '-Wall', '-Wextra', *mutation,
                        '-I'+str(ROOT/'tools/viewport_stub'), '-I'+str(ROOT/'tools/hoststub'),
                        '-I'+str(ROOT/'tools/sessionstub'), '-I'+str(fonts),
                        '-I'+str(ROOT/'firmware-cardputer/src'), '-I'+str(ROOT/'protocol'),
                        *map(str, sources), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    main()
