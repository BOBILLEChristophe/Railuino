# Changelog

## Unreleased — phase 1: fixes

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
