# Setup and settings

## Flash

### From the browser (Chrome or Edge, nothing to install)

1. Download the firmware from the latest
   [release](../../../releases): `radar-auto-lock-usb.bin` (recommended) or
   `radar-auto-lock-ble.bin` (Bluetooth). Each push to `main` also builds both
   files under [Actions](../../../actions) → latest run → Artifacts.
2. Connect the board's **COM** port to your computer.
3. Open [espressif.github.io/esptool-js](https://espressif.github.io/esptool-js/),
   baud rate 460800, click **Connect** and pick the board's serial port.
4. Flash address `0x0`, choose the `.bin` file, click **Program**.
5. When it finishes, press the board's **RST** button.

If the board is not found: hold **BOOT**, tap **RST**, release **BOOT**, connect again.

### With ESP-IDF

```bash
git clone https://github.com/Kymopsis/radar-auto-lock.git
cd radar-auto-lock/firmware
idf.py build                       # USB keyboard
idf.py -p PORT flash monitor       # PORT = the COM port, e.g. COM5 or /dev/ttyUSB0
```

Bluetooth build:

```bash
idf.py -B build-ble -D SDKCONFIG=build-ble/sdkconfig \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.ble" build
```

Pins (radar UART, LED, button) can be changed in `idf.py menuconfig` →
**Radar Auto-Lock**.

## First start

Plug the **USB** port into the PC. Windows, macOS and Linux install it as a
standard keyboard. The LED tells you what is going on:

| LED | Meaning |
|---|---|
| Purple, blinking | Keyboard not connected (USB not plugged into a PC, or Bluetooth not paired) |
| Red, blinking | No data from the radar: check TX/RX and the 3.3 V supply |
| Green | You are at the desk |
| Blue | You are away (PC locked) |
| Amber | Paused |
| White flash | Lock (2×) or wake (1×) sent |

### Your OS

Hold **BOOT** for 3 s and release. The LED flashes cyan:
1× Windows (Win + L), 2× macOS (Ctrl + Cmd + Q), 3× Linux (Super + L).
Hold again to go to the next one. The choice is saved.

- **macOS** may open the Keyboard Setup Assistant the first time. Close it.
- **Linux:** Super + L locks GNOME, KDE Plasma and Cinnamon. On other desktops
  bind your screen locker to Super + L.

### Waking a sleeping PC (USB only)

The board asks the PC to wake up over USB. Windows allows this for keyboards by
default; if not, open Device Manager → Keyboards → the "Radar Auto-Lock"
HID keyboard → Power Management → tick "Allow this device to wake the
computer". Many laptops ignore USB wake when the lid is closed.

## Settings

Connect the **COM** port and open a serial terminal at **115200 baud**
(the esptool-js page has one: Console tab; or PuTTY, `screen`, Arduino Serial
Monitor). Type `help`.

| Command | Default | What it does |
|---|---|---|
| `status` | | Radar link, targets in the zone, keyboard state |
| `watch on` / `watch off` | off | Prints presence twice a second while you test |
| `set away 30` | 30 s | Lock after nobody is seen this long (5–600) |
| `set range 1.2` | 1.2 m | Only things closer than this count (0.3–3.0) |
| `set fov 60` | 60° | Only things within ± this angle count (10–60) |
| `set return 1000` | 1000 ms | Seen this long counts as "back" (200–5000) |
| `set cloud 3` | 3 | Point-cloud points in the zone that count as present; 0 = targets only |
| `set os macos` | windows | `windows`, `macos` or `linux` |
| `set lock off` | on | Pause auto-lock (same as a short BOOT press) |
| `set wake off` | on | Do not wake the screen when you come back |
| `lock`, `wake` | | Send the shortcut now: tests the keyboard link |
| `unpair` | | Forget Bluetooth pairings |
| `defaults` | | Back to the defaults |

Settings are saved on the board immediately.

### Tuning

1. Sit normally and type `watch on`. You should see `seen=1` most of the time.
2. Lean back, read, stay still for a minute. If `idle` climbs close to the
   `away` time, raise `set away` or `set cloud 2`.
3. Ask someone to walk behind your chair. If it shows `seen=1` while your seat
   is empty, lower `set range` (for example `0.9`) or `set fov 40`.

## Bluetooth

1. Flash `radar-auto-lock-ble.bin` and power the board from any USB charger.
2. Windows: Settings → Bluetooth & devices → Add device → Bluetooth →
   **Radar Auto-Lock**. macOS: System Settings → Bluetooth. No PIN.
3. To pair with another computer: type `unpair`, or hold **BOOT** 10 s
   (factory reset, also restores default settings).

Bluetooth cannot wake a PC that is asleep; it wakes the screen when the PC is
only locked.

## Factory reset

Hold **BOOT** for 10 s (three red flashes). Settings go back to the defaults
and Bluetooth pairings are removed.
