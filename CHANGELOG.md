# Changelog

## Unreleased

### Added
- Example `DesktopStation`: serial gateway for the Desktop Station software, from Uwe Monreal's pull request #1 (Serial_RS5_ESP32_LED, based on Yaasan's gateway), on the current library. Same commands, answers and events as the original; `reset()` and `getS88()` restored; ESP32, Uno and Mega.
- `MfxRegistry::assign(uid, address)`: records a known UID -> address pair (Loc-ID or SID); a later Bind of the MS2 replaces it.
- WebControl: locomotives can be given by their MFX UID (`data-uid` in `index.html`); the sketch follows the addresses given by the MS2 with an `MfxRegistry` (kept in NVS) and uses `data-address` until the UID is known. `GET /locos` returns the table as JSON. Answers give the address used.

### Changed
- `exchangeMessage()` also checks the UID of the answers to MFX Bind and Verify.
- WebControl: the speed slider sends at most one command every 100 ms while it moves, and always the final value when it is released (it sent one per pixel). Errors of the layout are shown in the log (HTTP 503) instead of being ignored.

## 0.13.0 — 2026-10-12 (phase 5: MFX registration)

### Added
- `MfxRegistry`: table of the MFX locomotives (UID -> SID -> Loc-ID), kept across restarts (`save()` / `load()` / `setAutoSave(true)`: Preferences on ESP32, EEPROM on AVR).
  - Observer mode (default), with an MS2 or CS2 on the bus: sends nothing, learns the addresses from the Bind messages of the MS2. `locIdOf(uid)` gives the current address of a locomotive even after the MS2 changed its SID.
  - Master mode, without MS2: Discovery -> Bind -> Verify with 3 tries, based on rocrail_mfx_discovery v0.4 by Christophe Bobille; same SID for a known locomotive; `forceReRegistration()`.
- `TrackController::poll()` also runs the registry; `locoDiscovery()` starts a discovery through it.
- Examples `03.Mfx/MfxObserver` and `03.Mfx/MfxMaster`.
- 5 unit tests (29 in total), including the frames seen on the layout.

### Changed
- `setPower(true)` no longer sends the MFX re-registration counter (value 3, inherited from 0.9): changing it made the locomotives register again, and an MS2 could then give them new SIDs. `setMfxCounterOnPowerOn(true)` restores the old behaviour.
- `locoDiscovery()` no longer sends a hard-coded UID and no longer blocks for 60 s.

### Removed
- `extras/archive/MFX_discovery.ino.txt`, replaced by `MfxRegistry`; the v0.4 program is kept in `extras/reference`.

## 0.12.0 — 2026-10-12 (phase 4: modern API)

### Added
- `MarklinProtocol.h`: protocol constants in the `Marklin` namespace (`Cmd::Speed`, `SysCmd::Go`, `Addr::MFX`, `Direction::Toggle`...). The `Config.h` macros stay for existing sketches.
- `TrackMessage::make()`, `makeSystem()`, `uid()` / `setUid()` (32 bit), `canId()` / `setCanId()`.
- `TrackController::onMessage()` and `poll()`: messages from the MS2, feedback events, pings are passed to a handler instead of being dropped, including those received while waiting for an answer.
- `TrackController::setLog(Print*)`: traces to any output (Serial1, TCP...).
- `setLocoDirection(address, Marklin::Direction)`.
- `CommandParser`: text commands with free spacing, hexadecimal addresses (`0x4007`), functions up to 31, `turnout`, `halt`, `version`, `help`, and "ok" / "error: ..." answers.
- Example `Monitor`: prints and decodes the MS2 commands, e.g. to read the current address of a locomotive.

### Changed
- Addresses are `uint32_t` (Loc-ID or MFX UID); existing calls are unchanged.
- No bare command bytes and no direct `Serial` output left in `TrackController`: traces go to the log (Serial in debug mode). Errors are only printed when a log is set; `begin()` still returns false.
- `handleUserCommands()` uses `CommandParser`; `SerialCommand` answers "ok" / "error".
- Getters accept `nullptr` outputs.

## 0.11.0 — 2026-10-12 (phase 3: CAN drivers, tests, CI)

### Added
- `CanDriver` interface (`src/CanDriver.h`) with two drivers: `Esp32TwaiDriver` (ESP32 built-in controller, ACAN_ESP32) and `Mcp2515Driver` (MCP2515 on SPI, ACAN2515), which now also works on ESP32.
- `TrackController(CanDriver&, hash, debug, timeout, loopback)` to choose the driver; `begin()` without pins.
- Example `02.Drivers/Mcp2515OnEsp32`.
- Unit tests with PlatformIO and Unity: `pio test -e native` (17 tests, simulated CAN bus).
- GitHub Actions: unit tests and every example on every push and pull request.

### Changed
- Existing sketches are unchanged: without a driver, `TrackController` uses the TWAI controller on ESP32 and an MCP2515 on AVR.
- The `RAILUINO_MCP2515_*` defaults moved to `Mcp2515Driver.h`.
- Standard (11 bit) CAN frames are ignored.

### Removed
- `extras/host_tests` (replaced by `pio test -e native`).

## 0.10.0 — 2026-10-12 (phase 2: library layout)

### Breaking changes
- Sketches include `<Railuino.h>` instead of `"Config.h"` and `"TrackController.h"` copied next to them.
- `src/main.cpp` and `data/` moved to the `WebControl` example; its `secrets.h` now lives in the example folder.
- `TrackReporterIOX.cpp` (empty) removed.

### Added
- `library.properties` and `library.json`: the repository installs as a library (Arduino IDE, PlatformIO `lib_deps`).
- `src/Railuino.h`, `RAILUINO_VERSION_STRING`.
- Examples `Speed` and `Turnout` (ported from 0.9); every example builds for ESP32, Uno and Mega, except `WebControl` and `TcpCommand` (ESP32).
- `extras/build_examples.sh`: builds every example with PlatformIO. Host tests also compile the examples.
- `platformio.ini` builds an example against `src/`; ESP32 platform pinned to pioarduino 55.03.312.

### Removed
- Library copies in `Test_address_loco/` and `Test_address_loco.zip`, `WebCommand`, PlatformIO `include/` template.
- `MFX_discovery.ino` moved to `extras/archive/` (kept for the MFX work).

## 0.9.6 — 2026-10-12 (phase 1: fixes)

Fixes from the code audit, without changing the architecture. Numbers refer to the audit findings.

### Breaking changes
- `TrackController(timeOut)` and `TrackController(timeOut, debug)` are removed (#1). The first parameter is always the hash, as in Railuino 0.9: `TrackController(hash, debug = false, timeOut = 1000, loopback = false)`. Use `setTimeout()` to change only the timeout.
- `begin()` returns `bool` (existing calls that ignore the result still compile).
- `TrackReporterS88(modules)` takes optional pin parameters; `refresh(modules)` now honours its parameter.

### Fixed
- #1 `TrackController ctrl(0xDF24)` and `ctrl(0xDF24, true)` set the timeout instead of the hash.
- #2 `exchangeMessage()` looped forever after `millis()` wrapped around (49.7 days).
- #3 `exchangeMessage()` accepted the answer for another locomotive or another system sub-command.
- #4 A send error froze the controller in debug mode.
- #5 `TrackReporterS88::refresh()` never returned from 16 modules on, and wrote past its buffer.
- #6 `TrackMessage::parseFrom()` truncated the hash to 8 bits.
- #7 `getVersion()` did not fill `high`/`low` when the MS2 answered first.
- #8 `begin()` sent the Gleisbox wake-up message twice and reported success after a CAN error.
- #9 Only Uno and Mega compiled among AVR boards; unsupported architectures now get an `#error`.
- #12 `setAccessory()` always returned `true`; `writeConfig()` used the short timeout.
- #13 `accessory` serial/TCP command always sent power 0.
- #15 `TrackMessage` is `Printable` again (`Serial.println(message)`).
- #21 Library files are LGPL 2.1 again (they carried the CC0 header of the examples); `LICENSE` added.

### Added
- WiFi credentials of `main.cpp` read from `src/secrets.h` (ignored by Git); template in `src/secrets.h.example`.
- `CAN_RX_PIN` / `CAN_TX_PIN` in `Config.h`, default pins of `begin()` on ESP32.
- `setTimeout()`, `getTimeout()`, `DEFAULT_TIMEOUT`, `CV_TIMEOUT`.
- `RAILUINO_MCP2515_QUARTZ`, `RAILUINO_MCP2515_CS`, `RAILUINO_MCP2515_INT` build flags for AVR (#10).
- Host tests in `extras/host_tests` (`sh extras/host_tests/run.sh`).

### Removed
- Unused receive ring buffer (`_buffer`, `dequeue`) (#17).
