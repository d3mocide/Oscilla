# Oscilla C5 + single-Wio carrier

Use this reference only for the C5-to-Wio carrier. The current electrical authority remains the project-root `Research/c5-backpack-design.md` Rev D.

## Required nets

| Net | C5 GPIO | Wio endpoint |
|---|---:|---|
| 3V3 | — | 3V3 |
| GND | — | GND |
| LORA_SCK | 8 | SCK |
| LORA_MISO | 9 | MISO |
| LORA_MOSI | 10 | MOSI |
| LORA_NSS | 23 | NSS |
| LORA_RESET | 1 | RST |
| LORA_DIO1 | 0 | DIO1 |
| LORA_BUSY | 24 | BUSY |
| LORA_RF_SW | 25 | RF_SW |

Leave Wio `VIN`, `D0`, `D6`, and `D7` unconnected. This is a custom carrier; never derive connections from the Wio's stacked-XIAO labels.

## External carrier components

| Reference purpose | Value | Connection |
|---|---:|---|
| Radio deselect pull | 10 kOhm | NSS to 3V3 |
| Reset release pull | 10 kOhm | RST to 3V3 |
| RF switch boot pull | 10 kOhm | RF_SW to GND |
| High-frequency bypass | 100 nF ceramic | Wio 3V3 to GND |
| Local supply support | 10 uF | Wio 3V3 to GND |

The direct-wire bench test deliberately omitted the three pulls and received real traffic, but it was not field-ready evidence. Fit these components for the PCB unless a current board-level measurement and design review establishes an intentional alternative.

## Design constraints

- RF_SW is GPIO25, a C5 strap-sensitive net. Cold boot and USB recovery with the radio attached are mandatory physical tests.
- BUSY and DIO1 are required functional signals.
- The Wio is powered from C5 3V3; do not add an independently switched radio supply without a design decision.
- Current headroom is not demonstrated. C5+Wio use is provisionally close to the XIAO 3V3-path budget; qualify final power at the module pins.
