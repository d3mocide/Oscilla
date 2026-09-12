# env.sh — put both toolchains on PATH. Source it, don't run it.

# --- ESP-IDF (probe) -------------------------------------------------------
OSCILLA_IDF_PATH="${OSCILLA_IDF_PATH:-$HOME/esp/esp-idf}"
if [ -f "$OSCILLA_IDF_PATH/export.sh" ]; then
    # Chatty, and nonzero in some shells; don't abort a caller under `set -e`.
    . "$OSCILLA_IDF_PATH/export.sh" >/dev/null 2>&1 || true
else
    echo "tools/env.sh: no ESP-IDF at $OSCILLA_IDF_PATH" >&2
fi

# --- PlatformIO (deck) -----------------------------------------------------
# APPEND, never prepend: this venv has its own python and would shadow
# ESP-IDF's, breaking idf.py. See AGENTS.md §5 gotcha 1.
OSCILLA_PIO_BIN="${OSCILLA_PIO_BIN:-$HOME/.platformio/penv/bin}"
if [ -x "$OSCILLA_PIO_BIN/pio" ]; then
    case ":$PATH:" in
        *":$OSCILLA_PIO_BIN:"*) ;;
        *) PATH="$PATH:$OSCILLA_PIO_BIN"; export PATH ;;
    esac
else
    echo "tools/env.sh: no PlatformIO at $OSCILLA_PIO_BIN" >&2
fi
