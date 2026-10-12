
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 – 2026 christophe bobille
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

#include "MfxRegistry.h"
#include "TrackController.h"
#include "MarklinProtocol.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <Preferences.h>
#elif defined(ARDUINO_ARCH_AVR)
#include <EEPROM.h>
#ifndef RAILUINO_MFX_EEPROM_ADDRESS
#define RAILUINO_MFX_EEPROM_ADDRESS 0
#endif
#endif

using namespace Marklin;

namespace
{
  const uint32_t STORAGE_MAGIC = 0x524D5831UL; // "RMX1"
  const uint8_t DISCOVERY_MFX = 0x20;          // as in v0.4
}

MfxRegistry::MfxRegistry(TrackController &controller, Mode mode)
    : mCtrl(controller), mMode(mode), mFirstSid(5), mCounter(1), mAutoSave(false),
      mHandler(nullptr), mHandlerContext(nullptr)
{
}

/* -------------------------------------------------------------------
   Start
-------------------------------------------------------------------  */

void MfxRegistry::begin()
{
  mCtrl.setMfxRegistry(this);

  if (mAutoSave)
    load();

  if (mMode != Mode::Master)
    return;

  // Start sequence of rocrail_mfx_discovery v0.4 (delays: setup only).
  TrackMessage m = TrackMessage::makeSystem(SysCmd::Stop);
  delay(1000);
  mCtrl.sendMessage(m);

  delay(1000);
  m = TrackMessage::make(Cmd::Bootloader, 0, 5);
  m.data[4] = GleisboxWakeUp;
  mCtrl.sendMessage(m);

  delay(1000);
  sendCounter();

  // Global Verify (UID 0, SID 0): unbinds every locomotive, which then
  // gets its SID again from us (the same one if it is in the table).
  delay(1000);
  m = TrackMessage::make(Cmd::Verify, 0, 6);
  mCtrl.sendMessage(m);

  delay(1000);
  m = TrackMessage::makeSystem(SysCmd::Go);
  mCtrl.sendMessage(m);

  delay(1000);
  startDiscovery();
}

bool MfxRegistry::startDiscovery()
{
  if (mMode != Mode::Master)
    return false;
  TrackMessage m = TrackMessage::make(Cmd::Discovery, 0, 1);
  m.data[0] = DISCOVERY_MFX;
  if (Print *out = mCtrl.getLog())
    out->println(F("MFX discovery started"));
  return mCtrl.sendMessage(m);
}

bool MfxRegistry::sendCounter()
{
  TrackMessage m = TrackMessage::makeSystem(SysCmd::MfxRegCounter, 0, 7);
  m.data[5] = highByte(mCounter);
  m.data[6] = lowByte(mCounter);
  return mCtrl.sendMessage(m);
}

bool MfxRegistry::forceReRegistration()
{
  if (mMode != Mode::Master)
    return false;
  mCounter++;
  if (mAutoSave)
    save();
  return sendCounter();
}

/* -------------------------------------------------------------------
   Table
-------------------------------------------------------------------  */

MfxRegistry::Entry *MfxRegistry::find(uint32_t uid)
{
  for (Entry &e : mEntries)
    if (e.state != State::Free && e.uid == uid)
      return &e;
  return nullptr;
}

MfxRegistry::Entry *MfxRegistry::freeSlot()
{
  for (Entry &e : mEntries)
    if (e.state == State::Free)
      return &e;
  return nullptr;
}

bool MfxRegistry::sidUsed(uint16_t sid, const Entry *except) const
{
  for (const Entry &e : mEntries)
    if (&e != except && e.state != State::Free && e.sid == sid)
      return true;
  return false;
}

uint16_t MfxRegistry::nextFreeSid() const
{
  uint16_t sid = mFirstSid;
  while (sidUsed(sid, nullptr) && sid < MAX_SID)
    sid++;
  return sid;
}

uint16_t MfxRegistry::locIdOf(uint32_t uid) const
{
  for (const Entry &e : mEntries)
    if (e.state == State::Confirmed && e.uid == uid)
      return Addr::MFX + e.sid;
  return 0;
}

uint32_t MfxRegistry::uidOf(uint16_t locIdOrSid) const
{
  const uint16_t sid = locIdOrSid >= Addr::MFX && locIdOrSid < Addr::SX2 ? locIdOrSid - Addr::MFX : locIdOrSid;
  for (const Entry &e : mEntries)
    if (e.state == State::Confirmed && e.sid == sid)
      return e.uid;
  return 0;
}

bool MfxRegistry::assign(uint32_t uid, uint16_t locIdOrSid)
{
  const uint16_t sid = locIdOrSid >= Addr::MFX && locIdOrSid < Addr::SX2 ? locIdOrSid - Addr::MFX : locIdOrSid;
  if (uid == 0 || !isValidSid(sid))
    return false;
  learn(uid, sid);
  return locIdOf(uid) == Addr::MFX + sid;
}

uint8_t MfxRegistry::size() const
{
  uint8_t n = 0;
  for (const Entry &e : mEntries)
    if (e.state != State::Free)
      n++;
  return n;
}

const MfxRegistry::Entry &MfxRegistry::entry(uint8_t index) const
{
  static const Entry none;
  for (const Entry &e : mEntries)
    if (e.state != State::Free && index-- == 0)
      return e;
  return none;
}

void MfxRegistry::onChange(ChangeHandler handler, void *context)
{
  mHandler = handler;
  mHandlerContext = context;
}

void MfxRegistry::clear()
{
  for (Entry &e : mEntries)
    e = Entry();
  if (mAutoSave)
    save();
}

const char *MfxRegistry::stateName(State state)
{
  switch (state)
  {
  case State::Free: return "free";
  case State::New: return "new";
  case State::BindSent: return "bind sent";
  case State::VerifySent: return "verify sent";
  case State::Confirmed: return "confirmed";
  case State::Failed: return "FAILED";
  }
  return "?";
}

void MfxRegistry::printTo(Print &out) const
{
  out.println(F("---- MFX locomotives ----"));
  bool any = false;
  for (const Entry &e : mEntries)
  {
    if (e.state == State::Free)
      continue;
    any = true;
    out.print(F("UID "));
    TrackMessage::printHex(out, e.uid, 8);
    out.print(F("  SID "));
    out.print(e.sid);
    out.print(F("  Loc-ID 0x"));
    TrackMessage::printHex(out, Addr::MFX + e.sid, 4);
    out.print(F(" ("));
    out.print(static_cast<unsigned long>(Addr::MFX + e.sid));
    out.print(F(")  "));
    out.println(stateName(e.state));
  }
  if (!any)
    out.println(F("(none)"));
}

void MfxRegistry::log(const __FlashStringHelper *text, const Entry &e)
{
  Print *out = mCtrl.getLog();
  if (out == nullptr)
    return;
  out->print(text);
  out->print(F(" UID "));
  TrackMessage::printHex(*out, e.uid, 8);
  out->print(F("  SID "));
  out->print(e.sid);
  out->print(F("  Loc-ID 0x"));
  TrackMessage::printHex(*out, Addr::MFX + e.sid, 4);
  out->println();
}

void MfxRegistry::confirmed(Entry &e)
{
  e.state = State::Confirmed;
  e.tries = 0;
  log(F("MFX loco confirmed:"), e);
  if (mAutoSave)
    save();
  if (mHandler != nullptr)
    mHandler(e.uid, Addr::MFX + e.sid, mHandlerContext);
}

// A central station (the MS2, or us) bound 'uid' to 'sid'.
void MfxRegistry::learn(uint32_t uid, uint16_t sid)
{
  if (uid == 0 || !isValidSid(sid))
    return;

  Entry *e = find(uid);
  if (e != nullptr && e->state == State::Confirmed && e->sid == sid)
    return; // nothing new

  // Another locomotive that had this SID lost it.
  for (Entry &other : mEntries)
    if (&other != e && other.state == State::Confirmed && other.sid == sid)
      other.state = State::Failed;

  if (e == nullptr)
    e = freeSlot();
  if (e == nullptr)
  {
    if (Print *out = mCtrl.getLog())
      out->println(F("[!] MFX table full"));
    return;
  }
  e->uid = uid;
  e->sid = sid;
  confirmed(*e);
}

/* -------------------------------------------------------------------
   Received messages
-------------------------------------------------------------------  */

void MfxRegistry::handle(const TrackMessage &m)
{
  switch (m.command)
  {
  case Cmd::Bind:
    // Bind sent by an MS2/CS2 (request) or acknowledged by the Gleisbox
    // (response): UID in data[0..3], SID in data[4..5].
    if (m.length >= 6)
    {
      const uint32_t uid = m.uid();
      const uint16_t sid = (static_cast<uint16_t>(m.data[4]) << 8) | m.data[5];
      Entry *e = find(uid);
      const bool ourBind = mMode == Mode::Master && e != nullptr &&
                           (e->state == State::BindSent || e->state == State::VerifySent || e->state == State::New);
      if (ourBind)
        return; // wait for our Verify
      if (mMode == Mode::Master && !m.response && m.hash != mCtrl.getHash())
      {
        if (Print *out = mCtrl.getLog())
          out->println(F("[!] Another central station binds MFX locos: use Observer mode"));
      }
      learn(uid, sid);
    }
    break;

  case Cmd::Verify:
    // Answer to a Verify: UID, SID and, with 7 bytes, data[6] = 1 when
    // the locomotive answered.
    if (m.response && m.length >= 6)
    {
      const uint32_t uid = m.uid();
      const uint16_t sid = (static_cast<uint16_t>(m.data[4]) << 8) | m.data[5];
      const bool answered = m.length < 7 || m.data[6] != 0;
      Entry *e = find(uid);
      if (e == nullptr || sid == 0 || !answered)
        return; // e.g. a Verify with SID 0 from another central station
      if (sid != e->sid)
      {
        if (e->state != State::Confirmed)
          log(F("[!] Verify: other SID for"), *e);
        return;
      }
      if (e->state == State::VerifySent || e->state == State::BindSent)
        confirmed(*e);
    }
    break;

  case Cmd::Discovery:
    // A locomotive found by a discovery: UID in data[0..3].
    if (mMode == Mode::Master && m.response && m.length == 5)
    {
      const uint32_t uid = m.uid();
      if (uid == 0)
        return;
      Entry *e = find(uid);
      if (e != nullptr)
      {
        // Known locomotive announcing itself again: it lost its SID,
        // give it the same one again.
        if (e->state == State::Confirmed || e->state == State::Failed)
        {
          e->tries = 0;
          e->state = State::New;
          log(F("MFX loco known, bound again:"), *e);
        }
        return;
      }
      e = freeSlot();
      if (e == nullptr)
      {
        if (Print *out = mCtrl.getLog())
          out->println(F("[!] MFX table full"));
        return;
      }
      e->uid = uid;
      e->sid = nextFreeSid();
      e->tries = 0;
      e->state = State::New;
      log(F("New MFX loco:"), *e);
    }
    break;
  }
}

/* -------------------------------------------------------------------
   Master state machine (non blocking)
-------------------------------------------------------------------  */

bool MfxRegistry::sendUidSid(uint8_t command, const Entry &e)
{
  TrackMessage m = TrackMessage::make(command, e.uid, 6);
  m.data[4] = highByte(e.sid);
  m.data[5] = lowByte(e.sid);
  return mCtrl.sendMessage(m);
}

void MfxRegistry::service()
{
  if (mMode != Mode::Master)
    return;

  const uint32_t now = millis();
  for (Entry &e : mEntries)
  {
    switch (e.state)
    {
    case State::New:
      sendUidSid(Cmd::Bind, e);
      e.lastSendMs = now;
      e.state = State::BindSent;
      break;

    case State::BindSent:
      if (now - e.lastSendMs >= BIND_TO_VERIFY_MS)
      {
        sendUidSid(Cmd::Verify, e);
        e.lastSendMs = now;
        e.state = State::VerifySent;
      }
      break;

    case State::VerifySent:
      if (now - e.lastSendMs >= VERIFY_TIMEOUT_MS)
      {
        if (++e.tries < MAX_TRIES)
        {
          log(F("No Verify answer, new try:"), e);
          e.state = State::New;
        }
        else
        {
          log(F("[!] No confirmation after 3 tries:"), e);
          e.state = State::Failed;
        }
      }
      break;

    default:
      break;
    }
  }
}

/* -------------------------------------------------------------------
   Non volatile storage
-------------------------------------------------------------------  */

namespace
{
  struct Stored
  {
    uint32_t uid;
    uint16_t sid;
  };
}

#if defined(ARDUINO_ARCH_ESP32)

bool MfxRegistry::save()
{
  Stored table[MAX_LOCOS];
  uint8_t n = 0;
  for (const Entry &e : mEntries)
    if (e.state == State::Confirmed)
      table[n++] = {e.uid, e.sid};

  Preferences prefs;
  if (!prefs.begin("railuino", false))
    return false;
  prefs.putUShort("mfxCounter", mCounter);
  prefs.putUChar("mfxCount", n);
  const bool ok = prefs.putBytes("mfxTable", table, n * sizeof(Stored)) == n * sizeof(Stored);
  prefs.end();
  return ok;
}

bool MfxRegistry::load()
{
  Preferences prefs;
  if (!prefs.begin("railuino", true))
    return false;
  mCounter = prefs.getUShort("mfxCounter", mCounter);
  uint8_t n = prefs.getUChar("mfxCount", 0);
  if (n > MAX_LOCOS)
    n = MAX_LOCOS;
  Stored table[MAX_LOCOS];
  const size_t got = n > 0 ? prefs.getBytes("mfxTable", table, n * sizeof(Stored)) : 0;
  prefs.end();
  if (got != n * sizeof(Stored))
    return false;
  for (uint8_t i = 0; i < n; i++)
  {
    if (table[i].uid == 0 || !isValidSid(table[i].sid))
      continue; // damaged entry
    Entry *e = find(table[i].uid);
    if (e == nullptr)
      e = freeSlot();
    if (e == nullptr)
      break;
    e->uid = table[i].uid;
    e->sid = table[i].sid;
    e->state = State::Confirmed;
  }
  return true;
}

#elif defined(ARDUINO_ARCH_AVR)

bool MfxRegistry::save()
{
  int address = RAILUINO_MFX_EEPROM_ADDRESS;
  uint8_t n = 0;
  for (const Entry &e : mEntries)
    if (e.state == State::Confirmed)
      n++;
  EEPROM.put(address, STORAGE_MAGIC);
  address += sizeof(STORAGE_MAGIC);
  EEPROM.put(address, mCounter);
  address += sizeof(mCounter);
  EEPROM.put(address, n);
  address += sizeof(n);
  for (const Entry &e : mEntries)
  {
    if (e.state != State::Confirmed)
      continue;
    const Stored s = {e.uid, e.sid};
    EEPROM.put(address, s);
    address += sizeof(s);
  }
  return true;
}

bool MfxRegistry::load()
{
  int address = RAILUINO_MFX_EEPROM_ADDRESS;
  uint32_t magic = 0;
  EEPROM.get(address, magic);
  if (magic != STORAGE_MAGIC)
    return false;
  address += sizeof(magic);
  EEPROM.get(address, mCounter);
  address += sizeof(mCounter);
  uint8_t n = 0;
  EEPROM.get(address, n);
  address += sizeof(n);
  if (n > MAX_LOCOS)
    return false;
  for (uint8_t i = 0; i < n; i++)
  {
    Stored s;
    EEPROM.get(address, s);
    address += sizeof(s);
    if (s.uid == 0 || !isValidSid(s.sid))
      continue; // damaged entry
    Entry *e = find(s.uid);
    if (e == nullptr)
      e = freeSlot();
    if (e == nullptr)
      break;
    e->uid = s.uid;
    e->sid = s.sid;
    e->state = State::Confirmed;
  }
  return true;
}

#else

// Other platforms (unit tests): no storage.
bool MfxRegistry::save() { return false; }
bool MfxRegistry::load() { return false; }

#endif
