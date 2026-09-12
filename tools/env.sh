# env.sh — put Oscilla's two toolchains on PATH.  Source it, don't run it:
#
#     source tools/env.sh
#
# Both toolchains are installed on this machine but neither is on PATH by
# default: ESP-IDF requires sourcing its own export.sh (it sets IDF_PATH and
# prepends a RISC-V cross-compiler), and PlatformIO lives inside the VS Code
# extension's private virtualenv. Anything that needs them non-interactively
# should source this file rather than hardcoding the paths again.

# --- ESP-IDF (the probe: firmware-c5) --------------------------------------
# v5.5.1 — the first release line with real ESP32-C5 support.
OSCILLA_IDF_PATH="${OSCILLA_IDF_PATH:-$HOME/esp/esp-idf}"
if [ -f "$OSCILLA_IDF_PATH/export.sh" ]; then
    # export.sh is chatty and returns nonzero in some shells; don't let it
    # abort a caller running under `set -e`.
    . "$OSCILLA_IDF_PATH/export.sh" >/dev/null 2>&1 || true
else
    echo "tools/env.sh: no ESP-IDF at $OSCILLA_IDF_PATH" >&2
fi

# --- PlatformIO (the deck: firmware-cardputer) -----------------------------
# Installed by the platformio.platformio-ide VS Code extension, in its own venv.
#
# APPEND, never prepend. That directory is a virtualenv and contains its own
# `python`/`python3`; putting it first shadows the interpreter ESP-IDF's
# export.sh just put on PATH, and idf.py then fails with a confusing
# "No module named 'esp_idf_monitor'". Appending is safe because pio's shebang
# is an absolute path into its own venv — it does not need to be found first.
OSCILLA_PIO_BIN="${OSCILLA_PIO_BIN:-$HOME/.platformio/penv/bin}"
if [ -x "$OSCILLA_PIO_BIN/pio" ]; then
    case ":$PATH:" in
        *":$OSCILLA_PIO_BIN:"*) ;;
        *) PATH="$PATH:$OSCILLA_PIO_BIN"; export PATH ;;
    esac
else
    echo "tools/env.sh: no PlatformIO at $OSCILLA_PIO_BIN" >&2
fi
