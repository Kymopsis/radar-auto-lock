# How it works

## Radar frames

The radar streams TinyFrame packets over UART:

```
SOF 0x01 | ID u16 BE | LEN u16 BE | TYPE u16 BE | header checksum | payload | payload checksum
```

- Type `0x0A04`: target list. Each item has x (left), y (forward, out of the
  antenna face) and, on 3D firmware, z (up), in metres, plus an id.
- Type `0x0A08`: point cloud with the same item layout.

Payload layout (2D 16 bytes, 3D 24 bytes, 20-byte variant per item) is
detected from the frame length. The UART starts at 115200 baud and steps
through 1382400, 921600, 460800 and 256000 if no valid frame arrives within
3 s. Code: `firmware/components/radar_link/`, host test in
`radar_link/test/`.

## The zone

A target or point counts when it is in front of the radar (y > 0), closer than
`range` (1.2 m) and within ±`fov` (60°) of straight ahead. Someone walking
past at 2 m, or the wall behind you, does not count.

"Seen" in a frame = at least one target in the zone, **or** at least `cloud`
(3) point-cloud points in the zone. Frames older than 0.6 s are ignored.

## State machine

```
            nobody seen for `away` (30 s)            seen for `return` (1 s)
 PRESENT ────────────────────────────────▶ AWAY ──────────────────────────▶ PRESENT
            sends the lock shortcut                  sends wake (Shift / USB remote wakeup)
```

- Wake is sent only when this absence was locked by the board (not while paused).
- No radar data = timers frozen. A cable that falls off never locks your PC.
- After boot it locks only after it has seen someone.
- Short gaps (< 0.4 s) do not break the "coming back" streak; a person walking
  past for a moment does not wake the PC.

Code: `firmware/components/presence/`, host test in `presence/test/`
(leave and return, sitting still with sparse detections, radar dropout,
paused, passer-by).

## Keyboard

USB build: TinyUSB HID boot keyboard on the native USB port, with remote
wakeup enabled in the configuration descriptor. The board uses Espressif's
development USB vendor ID, which is fine for personal use; a product needs its
own VID/PID.

Bluetooth build: NimBLE HID-over-GATT keyboard, "just works" pairing with
bonding, the same 8-byte report.

Lock: modifiers down, key down, all up, 30 ms apart. Wake: a Shift tap if the
PC is awake (turns the screen on), USB remote wakeup if the USB bus is
suspended.
