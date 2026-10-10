#!/usr/bin/env python3
"""Compare compiled firmware colors with the rendered guide's CSS tokens."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
guide = (root / 'docs/brand/oscilla-master-brand-ui-guide.html').read_text()
css = dict(re.findall(r'--([\w-]+):\s*#([0-9a-fA-F]{6})', guide))
tokens = {'kVoidInk':'void', 'kPaperPhosphor':'cream', 'kFieldGreen':'green',
          'kSignalPink':'pink', 'kCalibrationYellow':'yellow', 'kFaultRed':'red',
          'kMutedSlate':'muted', 'kLineBorder':'line', 'kDimGreen':'dim',
          'kTrackDark':'dim-dark', 'kPanelDark':'panel'}
source = '#include "ui/theme.h"\n'
for name, token in tokens.items():
    rgb = int(css[token], 16)
    r, g, b = rgb >> 16, (rgb >> 8) & 255, rgb & 255
    expected = ((r // 8) << 11) | ((g // 4) << 5) | (b // 8)
    source += f'static_assert(ui::{name} == {expected}, "CSS palette drift: {token}");\n'
source += 'int main() {}\n'
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / 'palette.cpp'
    path.write_text(source)
    subprocess.run(['g++', '-std=c++17', '-I'+str(root/'firmware-cardputer/src'),
                    str(path), '-o', str(Path(directory)/'palette')], check=True)
print(f'UI palette: {len(tokens)} compiled tokens match master guide CSS')
