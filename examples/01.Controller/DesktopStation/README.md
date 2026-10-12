# Desktop Station serial gateway (ESP32)

Migration of the `Serial_RS5_ESP32_LED` example from [Uwe Monreal's pull request #1](https://github.com/BOBILLEChristophe/Railuino/pull/1), itself based on Yaasan's 2013 gateway.

This is **an adaptation for Railuino 0.10.0**, not the unchanged original. The old duplicated `TrackController` / `TrackMessage` files are intentionally excluded: the sketch includes the current `<Railuino.h>`.

## Hardware and software

- ESP32 with a CAN transceiver connected to a Märklin 250 kbit/s CAN bus.
- Default CAN pins in the sketch: RX GPIO 22, TX GPIO 23. Change for your hardware.
- Desktop Station via USB Serial at 115200 baud. **Not yet tested end-to-end.**
- Optional M5Atom LED display is not included; the example should work on a generic ESP32.

## Build

From the Railuino repository root (after copying this folder and `platformio.desktop-station.ini`):

```bash
pio run -c platformio.desktop-station.ini -e esp32dev
pio run -c platformio.desktop-station.ini -e esp32dev -t upload
pio device monitor -b 115200
```

## Supported serial commands

```
setPower(1)
setPower(0)
setLocoSpeed(16391,100)
setLocoDirection(16391,1)
toggleLocoDirection(16391)
setLocoFunction(16391,0,1)
toggleLocoFunction(16391,0)
setTurnout(12289,1)
getVersion()
getLocoConfig(16391,1)
setLocoConfig(16391,1,3)
setPing()
mfxDiscovery()
mfxBind(4660,22136,7)
mfxVerify(4660,22136,7)
```

Address values include Märklin protocol base (`0x4000|SID` for MFX). The two UID words of `mfxBind` / `mfxVerify` follow Uwe's original ordering; confirm on the wire before using on locomotives.

Responses: `100 Ready`, `200 Ok`, `300 Command error`, `301 Syntax error`, `302 receive timeout` (also used when an exchange fails), and `303 CAN initialization error`. Events: `@PING`, `@PWR`, `@DIR`, `@SPD`, `@FNC`, `@ACC`, `@MFX`, `@MFXBIND`.

## Not yet restored

- `getS88()` / `TrackReporterS88_DS` (Uwe's Desktop Station-specific adaptation): pinout and electrical interface require validation. The current core's `TrackReporterS88` is not a drop-in replacement until checked.
- M5Atom LED indication (`M5Atom.h` dependency), intentionally optional.
- `reset()` command: requires a proper CAN lifecycle implementation to avoid reinitializing an already-active controller.

## Safety and validation

- Never test traction/accessory commands on an unattended layout.
- Validate Desktop Station's expected syntax and formatting with a serial capture; compatibility has not been certified.
- `mfxDiscovery`, `mfxBind` and `mfxVerify` operate on MFX registration. Test on a dedicated section and verify UID/SID ordering.
- This example is not a complete implementation of the original PR. Keep it in a feature branch until tested.
