# Desktop Station serial gateway

Lets the [Desktop Station](https://desktopstation.net/wiki/doku.php/desktop_station_software#download) software drive a Märklin layout (Gleisbox) through an ESP32, an Uno or a Mega, connected over USB at 115200 baud.

Based on `Serial_RS5_ESP32_LED` by Uwe Monreal ([pull request #1](https://github.com/BOBILLEChristophe/Railuino/pull/1)), itself based on Yaasan's 2013 gateway. The sketch uses the current library (`#include <Railuino.h>`) instead of the copies of `TrackController` / `TrackMessage` of the pull request.

## Build

In `platformio.ini`, set:

```ini
src_dir = examples/01.Controller/DesktopStation
```

then `pio run -e esp32dev -t upload` (or `-e uno`, `-e megaatmega2560`). With the Arduino IDE: *File → Examples → Railuino → 01.Controller → DesktopStation*.

Default CAN pins on ESP32: Rx 22, Tx 23 (Uwe Monreal's board used 19 / 22). On Uno / Mega the MCP2515 settings come from `RAILUINO_MCP2515_*`.

Close the serial monitor before starting Desktop Station: only one program can use the port.

## Commands and answers

Same syntax and answers as Uwe Monreal's gateway, which was used with Desktop Station:

| Command | Answer |
| --- | --- |
| `setPower(1)` / `setPower(0)` | `200 Ok` |
| `setLocoSpeed(16391,100)` or `setLocoSpeed(16391,100,0)` | `200 Ok` (the third value, the speed step sent by Desktop Station, is ignored; speed 0..1000, higher values are limited to 1000) |
| `setLocoDirection(16391,1)`, `toggleLocoDirection(16391)` | `200 Ok` |
| `setLocoFunction(16391,0,1)`, `toggleLocoFunction(16391,0)` | `200 Ok` |
| `setTurnout(12289,1)` | `200 Ok` |
| `getVersion()` | `@VER,12F,` then `200 Ok` |
| `getLocoConfig(16391,1)` | `@CV,16391,1,value,` then `200 Ok` |
| `setLocoConfig(16391,1,3)` | `200 Ok` |
| `setPing()` | `200 Ok`, then `@PING,...` events |
| `getS88(1)` | `@S88,xxxx,` then `200 Ok` (only with `S88_ENABLED 1`) |
| `reset()` | `100 Ready` then `200 Ok` |
| `mfxDiscovery()`, `mfxBind(UID_HI,UID_LO,SID)`, `mfxVerify(...)` | `200 Ok` |

- Failure or unknown command: `300 Command error`; wrong syntax: `301 Syntax error`; line not finished after 4 s: `302 receive timeout`; CAN not started: `303 Unknown error`.
- Events from the bus: `@PING`, `@PWR`, `@DIR`, `@SPD`, `@FNC`, `@ACC`, `@MFX`, `@MFXBIND`, in hexadecimal without leading zeros, as the original.
- `mfxBind(65513,45460,7)` sends `FF E9 B1 94 00 07`: the UID words in the order they are written (`FFE9`, `B194`), as the original.

## Differences from the pull request

- `reset()` empties the receive queue and answers `100 Ready`, without restarting the CAN controller.
- `getS88()` uses the library's `TrackReporterS88`. It is disabled by default (`S88_ENABLED 0`): the S88 pins of Uwe Monreal's board (DATA 33, CLOCK 25, LOAD 23, RESET 21) include GPIO 23, used here for CAN Tx. Choose free pins before enabling it.
- The M5Atom LED (red / blue power indicator) is not included.
- Bus events are also reported when they arrive while a command waits for its answer.

## Not tested yet

The gateway compiles for ESP32, Uno and Mega and has been checked with a simulated bus, but not yet with Desktop Station on a real layout. Do not use an `MfxRegistry` with it: Desktop Station registers the MFX locomotives itself.
