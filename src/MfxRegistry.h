
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
 * Copyright (C) 2026 Uwe Monreal
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

#ifndef RAILUINO_MFX_REGISTRY_H
#define RAILUINO_MFX_REGISTRY_H

#include <Arduino.h>
#include "TrackMessage.h"

class TrackController;

/**
 * Table of the MFX locomotives: 32 bit UID (fixed, written in the
 * decoder) -> SID (short address given by a central station). The
 * Loc-ID used to drive a locomotive is 0x4000 + SID.
 *
 * Two modes:
 *
 * - Observer (default), when an MS2 or a CS2 is on the bus: the MS2
 *   registers the locomotives itself. The registry sends nothing; it
 *   learns the UID -> SID pairs from the Bind messages of the MS2, so a
 *   locomotive can be found by its UID even after the MS2 gave it a new
 *   SID:  ctrl.setLocoSpeed(mfx.locIdOf(0xFFE9B194), 300);
 *
 * - Master, when Railuino is the only central station (ESP32 + Gleisbox,
 *   no MS2): the registry runs the MFX registration itself (Discovery ->
 *   Bind -> Verify, 3 tries), as rocrail_mfx_discovery v0.4 by
 *   Christophe Bobille. Do NOT use it with an MS2 on the bus: both
 *   would give SIDs to the same locomotives.
 *
 * Usage:
 *   MfxRegistry mfx(ctrl);                 // or (ctrl, MfxRegistry::Mode::Master)
 *   setup(): ctrl.begin(...); mfx.begin();
 *   loop():  ctrl.poll();                  // also runs the registry
 *
 * The table can be kept across restarts: save() / load(), or
 * setAutoSave(true). ESP32: Preferences (NVS); AVR: EEPROM at
 * RAILUINO_MFX_EEPROM_ADDRESS (default 0).
 */
class MfxRegistry
{
public:
  enum class Mode : uint8_t
  {
    Observer,
    Master
  };

  enum class State : uint8_t
  {
    Free,       ///< empty slot
    New,        ///< discovered, Bind to send (master)
    BindSent,   ///< Bind sent, Verify to send (master)
    VerifySent, ///< Verify sent, waiting for the answer (master)
    Confirmed,  ///< UID -> SID known and valid
    Failed      ///< no confirmation after MAX_TRIES (master)
  };

  struct Entry
  {
    uint32_t uid = 0;
    uint16_t sid = 0;
    State state = State::Free;
    uint8_t tries = 0;
    uint32_t lastSendMs = 0;
  };

#if defined(ARDUINO_ARCH_AVR)
  static const uint8_t MAX_LOCOS = 16;
#else
  static const uint8_t MAX_LOCOS = 64;
#endif
  static const uint8_t MAX_TRIES = 3;
  static const uint32_t BIND_TO_VERIFY_MS = 20;
  static const uint32_t VERIFY_TIMEOUT_MS = 1000;

  /**
   * Called when a locomotive gets a (new) SID: uid, Loc-ID (0x4000 + SID).
   */
  typedef void (*ChangeHandler)(uint32_t uid, uint16_t locId, void *context);

  explicit MfxRegistry(TrackController &controller, Mode mode = Mode::Observer);

  /**
   * Attaches the registry to the controller (call after ctrl.begin()).
   * Loads the saved table if auto-save is on. In master mode, also runs
   * the start sequence (about 5 s, blocking) and starts a discovery.
   */
  void begin();

  Mode mode() const { return mMode; }

  /**
   * Master mode: asks the Gleisbox to search for MFX locomotives.
   */
  bool startDiscovery();

  /**
   * Master mode: changes the MFX re-registration counter, so that every
   * locomotive announces itself again and gets its SID again.
   */
  bool forceReRegistration();

  /**
   * Runs the master state machine (Bind, Verify, retries). Called by
   * TrackController::poll(); call it yourself if you do not use poll().
   */
  void service();

  /**
   * Learns from a received message. Called by TrackController.
   */
  void handle(const TrackMessage &message);

  /**
   * Loc-ID (0x4000 + SID) of a locomotive, 0 if unknown or not confirmed.
   */
  uint16_t locIdOf(uint32_t uid) const;

  /**
   * UID of the locomotive that has this Loc-ID (or SID), 0 if unknown.
   */
  uint32_t uidOf(uint16_t locIdOrSid) const;

  /**
   * Records by hand that 'uid' has this Loc-ID (0x4000 + SID) or SID,
   * e.g. read on the MS2. Later Bind messages of the MS2 update it.
   * Returns false if the table is full or the values are 0.
   */
  bool assign(uint32_t uid, uint16_t locIdOrSid);

  /**
   * Number of used entries, and access to them (0 .. size() - 1).
   */
  uint8_t size() const;
  const Entry &entry(uint8_t index) const;

  /**
   * Prints the table: UID, SID, Loc-ID, state.
   */
  void printTo(Print &out) const;

  void onChange(ChangeHandler handler, void *context = nullptr);

  /**
   * First SID given in master mode (default 5, as v0.4).
   */
  void setFirstSid(uint16_t sid) { mFirstSid = sid; }

  /**
   * Keeps the confirmed entries in non volatile memory.
   */
  bool save();
  bool load();
  void setAutoSave(bool on) { mAutoSave = on; }

  /**
   * Forgets every locomotive (the saved table too if auto-save is on).
   */
  void clear();

  static const char *stateName(State state);

private:
  TrackController &mCtrl;
  Mode mMode;
  Entry mEntries[MAX_LOCOS];
  uint16_t mFirstSid;
  uint16_t mCounter;
  bool mAutoSave;
  ChangeHandler mHandler;
  void *mHandlerContext;

  Entry *find(uint32_t uid);
  Entry *freeSlot();
  uint16_t nextFreeSid() const;
  bool sidUsed(uint16_t sid, const Entry *except) const;
  void learn(uint32_t uid, uint16_t sid);
  void confirmed(Entry &e);
  bool sendUidSid(uint8_t command, const Entry &e);
  bool sendCounter();
  void log(const __FlashStringHelper *text, const Entry &e);
};

#endif // RAILUINO_MFX_REGISTRY_H
