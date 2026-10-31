# FAQ

**The LED blinks red.**
No valid radar frames. Check TX ↔ RX are crossed (GPIO17 → radar RX0, GPIO18 ←
radar TX0), the radar has its own 3.3 V from the AMS1117 module, and GND is
shared. `status` shows `bytes`: if it stays at 0, nothing arrives at GPIO18.
If bytes grow but frames stay at 0, the baud rate is hunting; wait 15 s.

**It locks while I am sitting there.**
Raise `set away` (for example 60) and try `set cloud 2`. Make sure the radar
looks at your chest/head: on a very tall monitor tilt it down a little, under a
monitor use the desk foot (tilts up 6°).

**It does not lock when I leave.**
Something else is in the zone: a chair back, a plant, a person at the next
desk. `watch on` with the seat empty shows what is seen. Lower `set range` or
`set fov`.

**The LED blinks purple.**
USB build: the board's **USB** port (not COM) must go to the PC.
Bluetooth build: pair it first.

**The screen does not wake when I come back.**
`set wake on`. On Windows with "Windows Hello face", the camera then signs you
in. If the PC was asleep, see [waking a sleeping PC](setup.md#waking-a-sleeping-pc-usb-only).

**Win + L does nothing on my work PC.**
Your IT may have disabled locking with Win + L (Group Policy
"DisableLockWorkstation"). The board can only press keys; ask IT or use a
personal PC.

**Is this secure?**
It locks your PC for you; it cannot unlock it and never types anything except
the lock shortcut (Win + L, Ctrl + Cmd + Q or Super + L) and a Shift tap. Treat it as a convenience. Lock manually when it
matters.

**Can it see through the case?**
Yes, 60 GHz passes through thin PLA/PETG. The window in front of the antennas
is 1.5 mm, about half a wavelength in the plastic. Do not use metal-filled or
carbon-filled filament.

**Which radar firmware?**
The one pre-flashed on the Kymopsis presence & tracking kit. Any firmware that
outputs the TinyFrame target list (`0x0A04`) works.
