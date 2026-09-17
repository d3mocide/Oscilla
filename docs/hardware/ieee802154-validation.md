# 802.15.4 passive-validation record

**Date:** 2026-09-16
**Scope:** D-17 passive 802.15.4 MAC observation only

This record describes bench evidence, not a Zigbee, Thread, or topology
claim. All Oscilla firmware remained receive-only; a separate, temporary C5
running Espressif's IEEE 802.15.4 CLI supplied controlled traffic.

## Equipment and boundary

- Oscilla probe: XIAO ESP32-C5 `38:44:BE:1F:4F:A0` on its normal Grove UART.
- Deck: Cardputer ADV `50:78:7D:CE:6D:64`, temporarily flashed as the existing
  USB-to-Grove bridge, then restored to its normal application.
- Source: separate ESP32-C5 `38:44:BE:1F:55:FC`, temporary ESP-IDF 5.5.1 CLI.
- Observer: separate ESP32-C5 `10:BD:A3:CF:35:38`, temporary ESP-IDF CLI in
  receive mode.
- All boards used separate USB power; Grove 5 V was disconnected. The source
  sent one CCA-gated frame at a time and was configured for -80 dBm.

The external C5 images are bench instruments, not Oscilla firmware. Oscilla
has no transmit verb or transmit-capable driver call.

## MAC-capture and control-path result

With Oscilla fixed on channel 11, the source sent one 11-byte data frame with
PAN `1a2b` and short source `1234`. The source reported `Tx Done 11 bytes`.
Oscilla returned a `[ZIG]` table with `pan 1a2b`, `node 1234`, and `dropped=0`.
`stop phy` returned the 802.15.4 lane to idle. This demonstrates RF reception,
MAC parsing, table/OCP delivery, and stop integration.

## No-auto-ACK positive control

An ACK result requires a destination that would otherwise be eligible to ACK.
The external receiver was therefore set to PAN `1a2b` and short address
`0001`, with promiscuous mode disabled. An ACK-requested unicast (sequence
`0e`) produced `Rx ack 5 bytes` at the source.

For the comparison, a default-off `sdkconfig.acktest` overlay assigned the
same test identity to the Oscilla probe before `zig_radio` enabled
promiscuous RX. That mode disables ESP-IDF automatic ACK transmission. The
otherwise identical ACK-requested unicast (sequence `0f`) was seen by the
independent observer and captured by Oscilla as PAN `1a2b` / node `1234` with
zero drops. The source returned `ESP_IEEE802154_TX_ERR_NO_ACK` after its ACK
timeout. Normal probe and deck images were restored afterward.

This supports the receive-only boundary on air: a known-address Oscilla probe
captured the request but emitted no automatic ACK. It does not validate the
physical Mesh display, extended-loss behavior, Zigbee/Thread identity, or any
network-layer interpretation.
