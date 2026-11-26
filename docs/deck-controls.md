# Oscilla deck controls

Press **`h`** on the Cardputer to see the keys for the current card, dossier, or tool. Press **`h`** or **Back** (the backtick key) to close the guide. The companion TFT can keep showing the live view while you read.

## Get around

- **`,` / `/`** moves to the previous or next home card. **`;` / `.`** moves between rows on most lists.
- **Enter** opens the highlighted row or acts on the selected setting. **Back** returns one level. From **Link**, Back opens **Settings**.
- **`i`** opens System from a tool. **Back** returns to the tool. **`o`** opens saved Sessions.
- A sleeping display uses the first keypress to wake. Press the intended key again once the display is awake.

## Find a signal

1. On **WiFi Scan**, press **`r`** for a snapshot or **`c`** for live discovery. From **Link**, **`w`** also starts a Wi-Fi snapshot.
2. Highlight an AP with **`;` / `.`**, then press **`f`**. You can also press **Enter** to open the AP dossier and use **`f`** there.
3. For Bluetooth, take a snapshot with **`s`** on **BLE Scan**. Highlight a device and press **`f`**. This also works in a BLE dossier or on a selected **Anti-Surv** tracker.
4. In **Signal Hunt**, **`a`** toggles audio cues, **`r`** resets the peak, and **Back** exits. Faster, higher ticks indicate stronger received signal. Move a little and compare several readings.

## Observe and inspect

| View | Controls |
|---|---|
| **Link** | **`p`** pings the probe, **`s`** requests status, and **`r`** restarts the probe. **`w`** opens the Wi-Fi snapshot scan. |
| **WiFi Scan** | **`b`** saves a baseline or clears the active one. **Enter** opens the highlighted AP. In the dossier, **`r`** refreshes and **`x`** switches between radio details and observed clients. **`;` / `.`** selects a client on that page. |
| **BLE Scan** | **`s`** takes a snapshot, **`c`** starts or stops live discovery, and **`a`** toggles tracker watch. **Enter** opens the highlighted device. In the dossier, **`r`** refreshes and **`s`** stops an active BLE scan or refreshes when idle. |
| **802.15.4** | **`s`** starts or stops reception and **`x`** switches between PANs and nodes. Stopping can briefly restart the probe. |
| **Packet Monitor** | **`s`** starts or stops the survey. Highlight a channel and press **Enter** to monitor it. Use **Back** to return to the survey. |
| **Sniffer** | **`s`** starts or stops reception, **`x`** switches between clients and probes, and **Enter** opens the selected contact. |
| **Deauth Detect / Anti-Surv** | **`s`** starts or stops the passive watch. **`;` / `.`** selects an event or tracker. |

## LoRa, wardrive, and saved sessions

- **LoRa RX**. Press **`x`** to choose a receive profile, **`c`** to apply it, and **`s`** to start or stop listening. Enter opens the selected packet's hex view. The hex view uses **`;` / `.`** to page.
- **LoRa spectrum**. Press **`w`** from LoRa RX to switch modes. In the spectrum view, **`x`** cycles presets, **`s`** starts or stops the sweep, and **`r`** clears peak hold. **`w`** returns to packet mode.
- **OOK RX** (433 MHz, CC1101). Press **`s`** to start or stop listening and **`;` / `.`** to select a transmitter. Remotes are listed by their code; LaCrosse sensors by ID/channel with their latest reading.
- **GNSS**. The fix card before Wardrive; the external panel shows the fix reticle. **Enter** continues to Wardrive. The Link screen's GNSS row opens it.
- **Wardrive**. Press **`l`** to start or close the SD log. With an active Wardrive or LoRa log and a fresh GNSS fix, **`m`** saves a waypoint from those views.
- **Sessions**. Press **`o`** to open the browser. **`x`** cycles filters and **`r`** refreshes the list. Enter steps from a selected session into its summary, records, and available LoRa packet hex. Back unwinds one level.

## Settings

From **Link**, press **Back**. Select a row with **`;` / `.`**, then press **Enter** to change it, or use **`,` / `/`** to adjust in either direction. The menu covers brightness, idle sleep, Wi-Fi region, Wi-Fi band, LoRa profile, audio cues, and debug serial.

**Idle Sleep → OFF** keeps the displays awake. **Audio Cues** controls startup, Signal Hunt, and waypoint sounds. Changes are saved when the SD card is available. **`d`** also toggles debug serial from a regular card.
