/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 – 2026 christophe bobille
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#ifndef TRACK_REPORTER_H
#define TRACK_REPORTER_H

#include <Arduino.h>
#include "Config.h"

// ===================================================================
// === TrackReporterS88 ==============================================
// ===================================================================

/**
 * Implements the S88 bus protocol for reporting the state of tracks.
 * S88 is basically a long shift register where each bit corresponds
 * to a single contact on the track. Flip-flops on each S88 board make
 * sure activations are stored, so it is not necessary to query a
 * contact at the exact time it is activated. This implementation
 * allows a maximum of 512 bits or 32 full-width (16 bit) S88 boards.
 * The S88 standard recommends a maximum of 30 boards, so we should be
 * on the safe side.
 */

class TrackReporterS88
{

public:
  /**
   * Maximum number of 16 bit modules (512 contacts).
   */
  static constexpr uint8_t MAX_MODULES = 32;

  /**
   * Default pins, as in Railuino 0.9.
   * On ESP32, GPIO 4 (RESET) is also the default CAN Tx pin of
   * TrackController::begin(): pass other pins to the constructor.
   */
  static constexpr uint8_t DEFAULT_DATA_PIN = A0;
  static constexpr uint8_t DEFAULT_CLOCK_PIN = 2;
  static constexpr uint8_t DEFAULT_LOAD_PIN = 3;
  static constexpr uint8_t DEFAULT_RESET_PIN = 4;

private:
  /**
   * The number of 16 bit modules (1 to MAX_MODULES).
   */
  uint8_t mSize;

  /**
   * S88 bus pins.
   */
  uint8_t mDataPin;
  uint8_t mClockPin;
  uint8_t mLoadPin;
  uint8_t mResetPin;

  /**
   * The most recent contact values we know.
   */
  uint8_t mSwitches[MAX_MODULES * 2];

public:
  /**
   * Creates a new TrackReporter with the given number of modules
   * being attached. While this value can be safely set to the
   * maximum of 32, it makes sense to specify the actual number,
   * since this speeds up reporting. The method assumes 16 bit
   * modules. If you use 8 bit modules instead (or both) you need
   * to do the math yourself. Values above 32 are limited to 32.
   */
  explicit TrackReporterS88(uint8_t modules,
                            uint8_t dataPin = DEFAULT_DATA_PIN,
                            uint8_t clockPin = DEFAULT_CLOCK_PIN,
                            uint8_t loadPin = DEFAULT_LOAD_PIN,
                            uint8_t resetPin = DEFAULT_RESET_PIN);

  /**
   * Reads the current state of all contacts into the TrackReporter
   * and clears the flip-flops on all S88 boards. Call this method
   * periodically to have up-to-date values.
   */
  void refresh();

  /**
   * Same as refresh(), but reads only the first 'modules' modules.
   */
  void refresh(uint8_t modules);

  /**
   * Returns the state of an individual contact. Valid index values
   * are 1 to 512. Returns false for an index out of range.
   */
  bool getValue(uint16_t index) const;

  /**
   * Returns the state of 8 contacts. Valid index values
   * are 0 to 63. Returns 0 for an index out of range.
   */
  uint8_t getByte(uint8_t index) const;
};

#endif // TRACK_REPORTER_H
