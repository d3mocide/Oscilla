# OSCILLA_DECK_VER from the repo-root VERSION file, shared with the probe.
from pathlib import Path

Import("env")
ver = (Path(env.subst("$PROJECT_DIR")).parent / "VERSION").read_text().strip()
env.Append(CPPDEFINES=[("OSCILLA_DECK_VER", '\\"%s\\"' % ver)])
