# Wiring

Eight Dupont wires, no soldering (if your radar kit and AMS1117 module come
with pin headers).

```
PC USB port ──USB-C cable──▶ ESP32-S3 board, port "USB"   (power + keyboard)

ESP32-S3 board                      AMS1117-3.3 module          60 GHz radar (MA161BT)
  5V  ───────────────────────────▶  VIN
  GND ───────────────────────────▶  GND ────────────────────▶  GND (pin 14, and pin 9)
                                    3.3V OUT ───────────────▶  3.3V (pin 13)
  GPIO17 (TX) ───────────────────────────────────────────────▶  RX0 (pin 16)
  GPIO18 (RX) ◀───────────────────────────────────────────────  TX0 (pin 17)
  GPIO4 ─────────────────────────────────────────────────────▶  RST (pin 7)   optional
```

| ESP32-S3 pin | Goes to | Notes |
|---|---|---|
| 5V | AMS1117 VIN | 5 V from the PC's USB port, passed through by the board |
| GND | AMS1117 GND and radar GND | Use a separate wire to each |
| GPIO17 | Radar RX0 | ESP32 transmits |
| GPIO18 | Radar TX0 | ESP32 receives the radar data |
| GPIO4 | Radar RST | Optional. Lets the ESP32 restart the radar at boot |
| — | AMS1117 3.3V OUT → radar 3.3V | The radar's own supply |

Pin numbers are the radar module's pad numbers from the
[datasheet](https://kymopsis.com/product/60ghz-presence-tracking-radar-development-kit/?utm_source=github&utm_medium=docs&utm_campaign=radar-auto-lock).
If your kit comes with a labelled lead, follow the labels (3V3, GND, TX, RX).

## Rules that matter

- **Do not power the radar from the ESP32's 3V3 pin.** The radar draws short
  peaks of up to 600 mA; the board's regulator also feeds the ESP32 and the
  supply dips. The AMS1117 module gives the radar its own clean 3.3 V.
- **TX goes to RX.** ESP32 TX (GPIO17) to radar RX, ESP32 RX (GPIO18) to radar TX.
- **Use a USB 3 port or a powered hub** if the LED blinks red now and then:
  some laptop USB 2 ports cannot supply the radar's peaks.
- Leave GPIO 35, 36 and 37 alone on N16R8 boards (octal PSRAM).

## The two USB-C ports

| Port label | Used for |
|---|---|
| **USB** (native USB) | The keyboard. Plug this one into the PC you want to lock |
| **COM** (UART bridge) | Flashing the firmware and the settings console |

For everyday use only the **USB** port is connected. Plug in **COM** as well
when you want to flash or change settings; two cables to the same PC are fine.

## Into the case

1. Slide the radar into the slot behind the front window, antennas facing out
   (towards you), cable side up. It rests on the small ledge.
2. Plug the wires onto the ESP32 header pins, then lay the board in from the
   top, components up, USB-C ports into the opening on the right. The antenna
   end rests on the block on the left.
3. Tuck the AMS1117 module and the wires underneath the board.
4. Press the lid on. Its ribs hold the board and the radar in place.
5. Slide the clip (or the desk foot) into the dovetail under the case.

Nothing metal in front of the radar: no metallic paint, foil or a phone on top
of the window.
