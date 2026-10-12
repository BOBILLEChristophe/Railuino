
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

#include "TrackController.h"
#include "CommandParser.h"
#include "MfxRegistry.h"

#include "Esp32TwaiDriver.h"
#include "Mcp2515Driver.h"

/* -------------------------------------------------------------------
   Default CAN driver, used when no driver is given to the constructor:
   the built-in TWAI controller on ESP32, an MCP2515 on AVR
   (pins and quartz: RAILUINO_MCP2515_* in Mcp2515Driver.h).
-------------------------------------------------------------------  */

#if defined(ARDUINO_ARCH_ESP32)
static Esp32TwaiDriver sDefaultDriver;
static CanDriver *defaultDriver() { return &sDefaultDriver; }
#elif defined(ARDUINO_ARCH_AVR)
static Mcp2515Driver sDefaultDriver;
static CanDriver *defaultDriver() { return &sDefaultDriver; }
#else
// Other platforms (e.g. native unit tests): a driver must be given.
static CanDriver *defaultDriver() { return nullptr; }
#endif

/* -------------------------------------------------------------------
   TrackController (constructor / destructor)
-------------------------------------------------------------------  */

using namespace Marklin;

// Out-of-class definitions, required in C++11 if the constants are ODR-used.
constexpr uint32_t TrackController::DEFAULT_TIMEOUT;
constexpr uint32_t TrackController::CV_TIMEOUT;

// Default trace output in debug mode. The only place where the library
// refers to Serial; setLog() chooses another output.
static Print *defaultLog(bool debug) { return debug ? static_cast<Print *>(&Serial) : nullptr; }

static const char SEPARATOR[] = "------------------------------------------------------------------";

TrackController::TrackController()
    : mCan(defaultDriver()),
      mHash(0),
      mLog(nullptr),
      mLoopback(false),
      mTimeout(DEFAULT_TIMEOUT),
      mHandler(nullptr),
      mHandlerContext(nullptr),
      mMfx(nullptr),
      mMfxCounterOnPowerOn(false),
      mPendingAcc(),
      mAccAsync(false)
{
}

// Note: no output here. Global objects are constructed before setup()
// runs, i.e. before Serial.begin() has been called.
TrackController::TrackController(uint16_t hash, bool debug, uint32_t timeOut, bool loopback)
    : mCan(defaultDriver()),
      mHash(hash),
      mLog(defaultLog(debug)),
      mLoopback(loopback),
      mTimeout(timeOut),
      mHandler(nullptr),
      mHandlerContext(nullptr),
      mMfx(nullptr),
      mMfxCounterOnPowerOn(false),
      mPendingAcc(),
      mAccAsync(false)
{
}

TrackController::TrackController(CanDriver &can, uint16_t hash, bool debug, uint32_t timeOut, bool loopback)
    : mCan(&can),
      mHash(hash),
      mLog(defaultLog(debug)),
      mLoopback(loopback),
      mTimeout(timeOut),
      mHandler(nullptr),
      mHandlerContext(nullptr),
      mMfx(nullptr),
      mMfxCounterOnPowerOn(false),
      mPendingAcc(),
      mAccAsync(false)
{
}

TrackController::~TrackController()
{
}

/* -------------------------------------------------------------------
   Settings
-------------------------------------------------------------------  */

// Kept for compatibility with older versions.
void TrackController::init(uint16_t hash, bool debug, bool loopback, uint32_t timeOut)
{
    mHash = hash;
    mLog = defaultLog(debug);
    mLoopback = loopback;
    mTimeout = timeOut;
}

void TrackController::setTimeout(uint32_t timeOut) { mTimeout = timeOut; }
uint32_t TrackController::getTimeout() const { return mTimeout; }
uint16_t TrackController::getHash() const { return mHash; }
bool TrackController::isDebug() const { return mLog != nullptr; }
bool TrackController::isLoopback() const { return mLoopback; }
void TrackController::setLog(Print *log) { mLog = log; }
Print *TrackController::getLog() const { return mLog; }

void TrackController::onMessage(TrackMessageHandler handler, void *context)
{
    mHandler = handler;
    mHandlerContext = context;
}

void TrackController::setMfxRegistry(MfxRegistry *registry) { mMfx = registry; }
void TrackController::setMfxCounterOnPowerOn(bool on) { mMfxCounterOnPowerOn = on; }

void TrackController::dispatch(const TrackMessage &message)
{
    if (mMfx != nullptr)
        mMfx->handle(message);
    if (mHandler != nullptr)
        mHandler(message, mHandlerContext);
}

void TrackController::poll()
{
    TrackMessage message;
    while (receiveMessage(message))
        dispatch(message);
    if (mMfx != nullptr)
        mMfx->service();
    serviceAccessories(false);
}

/* -------------------------------------------------------------------
   TrackController::begin / end
-------------------------------------------------------------------  */

bool TrackController::begin(const uint8_t can_rx_pin, const uint8_t can_tx_pin)
{
    if (mCan == nullptr)
        return false;
    // Only meaningful for the ESP32 TWAI driver; ignored by the others.
    mCan->setPins(can_rx_pin, can_tx_pin);
    return begin();
}

bool TrackController::begin()
{
    if (mCan == nullptr)
    {
        if (mLog)
            mLog->println(F("!!! No CAN driver"));
        return false;
    }

    // The driver prints its configuration, or its error, to the log.
    if (!mCan->begin(mLog))
        return false;

    if (mLog)
        mLog->println(F("Configuration CAN OK"));

    delay(500);

    if (!mLoopback)
    {
        // Wakes up the Gleisbox when no MS2/CS is present. Sent once and
        // without waiting for an answer, as in Railuino 0.9: on a real
        // layout (Gleisbox 0x0011, firmware 1.47, with an MS2) the
        // Gleisbox does not answer this message.
        TrackMessage message = TrackMessage::make(Cmd::Bootloader, 0, 5);
        message.data[4] = GleisboxWakeUp;

        if (!sendMessage(message))
        {
            if (mLog)
                mLog->println(F("!!! Cannot send the Gleisbox wake-up message (0x1B 0x11)"));
            return false;
        }
    }

    if (mHash == 0)
        generateHash();

    return true;
}

void TrackController::end()
{
    // No coil left on.
    flushAccessories();

    // Discards the messages waiting in the CAN driver's receive FIFO.
    TrackMessage message;
    while (receiveMessage(message))
        ;
}

/* -------------------------------------------------------------------
   TrackController::sendMessage / receiveMessage
-------------------------------------------------------------------  */

void TrackController::logFrame(const char *direction, const CanFrame &frame)
{
    // Same layout as in the previous versions.
    mLog->print(direction);
    mLog->print(F(" ID : 0x"));
    mLog->println(frame.id, HEX);
    mLog->print(F("EXT : "));
    mLog->println(frame.ext ? F("extended") : F("standard"));
    mLog->print(F("RESP : "));
    mLog->println((frame.id >> 16) & 0x01);
    mLog->print(F("DLC : "));
    mLog->println(frame.len);
    mLog->print(F("COMMAND : 0x"));
    TrackMessage::printHex(*mLog, (frame.id >> 17) & 0xFF, 2);
    mLog->print(F("\nDATA : "));
    for (uint8_t i = 0; i < frame.len; i++)
    {
        mLog->print(F("0x"));
        TrackMessage::printHex(*mLog, frame.data[i], 2);
        if (i + 1 < frame.len)
            mLog->print(F(" - "));
    }
    mLog->println();
    mLog->println(SEPARATOR);
}

bool TrackController::sendMessage(TrackMessage &message)
{
    if (mCan == nullptr || message.length > 8)
        return false;

    message.hash = mHash;

    CanFrame frame;
    frame.id = message.canId();
    frame.ext = true;
    frame.len = message.length;
    for (uint8_t i = 0; i < message.length; i++)
        frame.data[i] = message.data[i];

    if (mLog)
        logFrame("<==", frame);

    return mCan->send(frame);
}

bool TrackController::receiveMessage(TrackMessage &message)
{
    if (mCan == nullptr)
        return false;

    CanFrame frame;
    bool result = mCan->receive(frame);

    // Märklin only uses extended identifiers: ignore standard frames.
    while (result && !frame.ext)
        result = mCan->receive(frame);

    if (!result)
        return false;

    message.clear();
    message.setCanId(frame.id);
    message.length = frame.len;
    for (uint8_t i = 0; i < frame.len; i++)
        message.data[i] = frame.data[i];

    if (mLog)
        logFrame("==>", frame);

    return true;
}

/* -------------------------------------------------------------------
   TrackController::exchangeMessage
-------------------------------------------------------------------  */

bool TrackController::isResponseTo(const TrackMessage &out, const TrackMessage &in)
{
    if (!in.response || in.command != out.command)
        return false;

    // Ignore responses carrying 4 bytes or less.
    if (in.length <= 4)
        return false;

    switch (out.command)
    {
    case Cmd::System: // UID in data[0..3], sub-command in data[4]
        if (out.length >= 5 && in.data[4] != out.data[4])
            return false;
        // fall through
    case Cmd::Bind:
    case Cmd::Verify:
    case Cmd::Speed:
    case Cmd::Direction:
    case Cmd::Function:
    case Cmd::ReadConfig:
    case Cmd::WriteConfig:
    case Cmd::Accessory:
        // The response must concern the same locomotive / accessory
        // (or the same MFX UID for Bind and Verify).
        // Without this check, the answer for another locomotive (sent
        // by an MS2 on the same bus) could be taken as ours.
        return in.uid() == out.uid();

    default: // Ping, Discovery, Bootloader...
        return true;
    }
}

bool TrackController::exchangeMessage(TrackMessage &out, TrackMessage &in, uint32_t timeout)
{
    // Keep a copy of the request: 'out' and 'in' may be the same object.
    const TrackMessage request = out;

    if (!sendMessage(out))
    {
        if (mLog)
            mLog->println(F("!!! Send error"));
        return false;
    }

    // Unsigned subtraction: stays correct when millis() wraps around.
    const uint32_t start = millis();

    while (millis() - start < timeout)
    {
        in.clear();
        if (receiveMessage(in))
        {
            if (mLog)
            {
                mLog->println(F("=== TRAME CAN RECUE ==="));
                in.printTo(*mLog);
                mLog->println();
            }
            if (isResponseTo(request, in))
                return true;
            // Not our answer: an MS2 command, a ping... it is not lost.
            dispatch(in);
        }
        yield();
    }

    if (mLog)
        mLog->println(F("!!! Receive timeout"));

    return false;
}

/* -------------------------------------------------------------------
   TrackController::generateHash
-------------------------------------------------------------------  */

void TrackController::generateHash()
{
    TrackMessage message;
    bool ok = false;

    while (!ok)
    {
        // Märklin rule: bits 7..9 of the hash are 0b110.
        mHash = (random(0x10000) & 0xFF7F) | 0x0300;

        if (mLog)
        {
            mLog->print(F("### Trying new hash 0x"));
            TrackMessage::printHex(*mLog, mHash, 4);
            mLog->println();
            mLog->println(SEPARATOR);
        }

        // Ping: every device answers with its own hash.
        message = TrackMessage::make(Cmd::Ping, 0, 0);
        sendMessage(message);

        delay(mTimeout);

        ok = true;
        while (receiveMessage(message))
        {
            if (message.hash == mHash)
                ok = false;
            dispatch(message);
        }
    }

    if (mLog)
    {
        mLog->println(F("### New hash looks good"));
        mLog->println(SEPARATOR);
    }
}

/* -------------------------------------------------------------------
   TrackController::locoDiscovery
-------------------------------------------------------------------  */

bool TrackController::locoDiscovery()
{
    return mMfx != nullptr && mMfx->startDiscovery();
}

/* -------------------------------------------------------------------
   Power and system commands
-------------------------------------------------------------------  */

bool TrackController::setPower(bool power)
{
    TrackMessage message = TrackMessage::makeSystem(power ? SysCmd::Go : SysCmd::Stop);

    if (!exchangeMessage(message, message, mTimeout))
    {
        if (mLog)
            mLog->println(F("Failed to send Power"));
        return false;
    }

    if (mLog)
    {
        mLog->println(power ? F("Power on") : F("Power off"));
        mLog->println(SEPARATOR);
    }

    if (power)
    {
        // MFX re-registration counter = 3, as in Railuino 0.9, only on
        // request: changing it makes the locomotives register again.
        if (mMfxCounterOnPowerOn)
        {
            message = TrackMessage::makeSystem(SysCmd::MfxRegCounter, 0, 7);
            message.data[6] = 0x03;
            if (!exchangeMessage(message, message, mTimeout))
            {
                if (mLog)
                    mLog->println(F("Failed to reset re-registration counter"));
                return false;
            }
        }

        // Enable MM2, MFX and DCC on the track.
        message = TrackMessage::makeSystem(SysCmd::TrackProtocol, 0, 6);
        message.data[5] = TrackProtocol::All;
        if (!exchangeMessage(message, message, mTimeout))
        {
            if (mLog)
                mLog->println(F("Failed to activate track protocol"));
            return false;
        }
    }
    return true;
}

bool TrackController::systemHalt(const uint32_t address)
{
    TrackMessage message = TrackMessage::makeSystem(SysCmd::Halt, address);
    return exchangeMessage(message, message, mTimeout);
}

bool TrackController::emergency(const uint32_t address)
{
    TrackMessage message = TrackMessage::makeSystem(SysCmd::LocoEmergency, address);
    return exchangeMessage(message, message, mTimeout);
}

/* -------------------------------------------------------------------
   Locomotives
-------------------------------------------------------------------  */

bool TrackController::setLocoDirection(const uint32_t address, uint8_t direction)
{
    TrackMessage message = TrackMessage::make(Cmd::Direction, address, 5);
    message.data[4] = direction;
    return exchangeMessage(message, message, mTimeout);
}

bool TrackController::setLocoDirection(const uint32_t address, Direction direction)
{
    return setLocoDirection(address, static_cast<uint8_t>(direction));
}

bool TrackController::toggleLocoDirection(const uint32_t address)
{
    return setLocoDirection(address, Direction::Toggle);
}

bool TrackController::getLocoDirection(const uint32_t address, uint8_t *direction)
{
    TrackMessage message = TrackMessage::make(Cmd::Direction, address, 4);
    if (!exchangeMessage(message, message, mTimeout))
        return false;
    if (direction != nullptr)
        *direction = message.data[4];
    return true;
}

bool TrackController::setLocoSpeed(const uint32_t address, uint16_t speed)
{
    TrackMessage message = TrackMessage::make(Cmd::Speed, address, 6);
    message.data[4] = highByte(speed);
    message.data[5] = lowByte(speed);
    return exchangeMessage(message, message, mTimeout);
}

bool TrackController::getLocoSpeed(const uint32_t address, uint16_t *speed)
{
    TrackMessage message = TrackMessage::make(Cmd::Speed, address, 4);
    if (!exchangeMessage(message, message, mTimeout))
        return false;
    if (speed != nullptr)
        *speed = (static_cast<uint16_t>(message.data[4]) << 8) | message.data[5];
    return true;
}

bool TrackController::setLocoFunction(const uint32_t address, uint8_t function, uint8_t power)
{
    TrackMessage message = TrackMessage::make(Cmd::Function, address, 6);
    message.data[4] = function;
    message.data[5] = power;
    return exchangeMessage(message, message, mTimeout);
}

bool TrackController::getLocoFunction(const uint32_t address, uint8_t function, uint8_t *power)
{
    TrackMessage message = TrackMessage::make(Cmd::Function, address, 5);
    message.data[4] = function;
    if (!exchangeMessage(message, message, mTimeout))
        return false;
    if (power != nullptr)
        *power = message.data[5];
    return true;
}

bool TrackController::toggleLocoFunction(const uint32_t address, uint8_t function)
{
    uint8_t power;
    if (getLocoFunction(address, function, &power))
        return setLocoFunction(address, function, power ? 0 : 1);
    return false;
}

/* -------------------------------------------------------------------
   Accessories
-------------------------------------------------------------------  */

bool TrackController::switchAccessoryOff(const uint32_t address, uint8_t position)
{
    // Power = 0 switches the coil off.
    TrackMessage message = TrackMessage::make(Cmd::Accessory, address, 6);
    message.data[4] = position;
    return exchangeMessage(message, message, mTimeout);
}

bool TrackController::setAccessory(const uint32_t address, uint8_t position, uint8_t power, uint16_t time)
{
    // A pulse still running on this accessory: switch it off first, so
    // that the two coils of a turnout are never on together.
    for (PendingAccessory &p : mPendingAcc)
    {
        if (p.used && p.address == address)
        {
            p.used = false;
            switchAccessoryOff(p.address, p.position);
        }
    }

    TrackMessage message = TrackMessage::make(Cmd::Accessory, address, 6);
    message.data[4] = position;
    message.data[5] = power;

    bool ok = exchangeMessage(message, message, mTimeout);

    if (time == 0)
        return ok;

    if (mAccAsync)
    {
        for (PendingAccessory &p : mPendingAcc)
        {
            if (!p.used)
            {
                p.used = true;
                p.address = address;
                p.position = position;
                p.dueMs = millis() + time;
                return ok;
            }
        }
        // No free slot: wait, as in blocking mode.
    }

    delay(time);

    // Switch the coil off. Always sent, even if the first message was not
    // acknowledged, so that a coil is never left on.
    return switchAccessoryOff(address, position) && ok;
}

void TrackController::serviceAccessories(bool all)
{
    const uint32_t now = millis();
    for (PendingAccessory &p : mPendingAcc)
    {
        // Signed difference: correct across the millis() wrap-around.
        if (p.used && (all || static_cast<int32_t>(now - p.dueMs) >= 0))
        {
            p.used = false;
            if (!switchAccessoryOff(p.address, p.position) && mLog)
                mLog->println(F("!!! Accessory switch-off not acknowledged"));
        }
    }
}

void TrackController::setAccessoryAsync(bool on)
{
    if (!on)
        flushAccessories();
    mAccAsync = on;
}

uint8_t TrackController::pendingAccessories() const
{
    uint8_t n = 0;
    for (const PendingAccessory &p : mPendingAcc)
        if (p.used)
            n++;
    return n;
}

void TrackController::flushAccessories()
{
    serviceAccessories(true);
}

bool TrackController::setTurnout(const uint32_t address, bool straight)
{
    return setAccessory(address, straight ? ACC_STRAIGHT : ACC_ROUND, 1, 1000);
}

bool TrackController::getAccessory(const uint32_t address, uint8_t *position, uint8_t *power)
{
    TrackMessage message = TrackMessage::make(Cmd::Accessory, address, 4);
    if (!exchangeMessage(message, message, mTimeout))
        return false;
    if (position != nullptr)
        *position = message.data[4];
    if (power != nullptr)
        *power = message.data[5];
    return true;
}

/* -------------------------------------------------------------------
   Decoder configuration (CV)
-------------------------------------------------------------------  */

bool TrackController::readConfig(const uint32_t address, uint16_t number, uint8_t *value)
{
    TrackMessage message = TrackMessage::make(Cmd::ReadConfig, address, 7);
    message.data[4] = highByte(number);
    message.data[5] = lowByte(number);
    message.data[6] = 0x01; // number of bytes to read

    if (!exchangeMessage(message, message, CV_TIMEOUT))
        return false;
    if (value != nullptr)
        *value = message.data[6];
    return true;
}

bool TrackController::writeConfig(const uint32_t address, uint16_t number, uint8_t value)
{
    TrackMessage message = TrackMessage::make(Cmd::WriteConfig, address, 8);
    message.prio = 0x01;
    message.data[4] = highByte(number);
    message.data[5] = lowByte(number);
    message.data[6] = value;

    // CV_TIMEOUT (10 s), as in Railuino 0.9: a write can take seconds.
    return exchangeMessage(message, message, CV_TIMEOUT);
}

/* -------------------------------------------------------------------
   TrackController::getVersion
-------------------------------------------------------------------  */

bool TrackController::getVersion(uint8_t *high, uint8_t *low)
{
    // Ping: every device on the bus answers with its software version in
    // data[4..5] and its device type in data[6..7]. The MS2 often answers
    // first, so we keep listening until a Gleisbox has answered or the
    // timeout expires.
    TrackMessage message = TrackMessage::make(Cmd::Ping, 0, 0);

    if (!sendMessage(message))
        return false;

    const uint32_t start = millis();
    while (millis() - start < mTimeout)
    {
        if (receiveMessage(message))
        {
            const uint16_t device = (static_cast<uint16_t>(message.data[6]) << 8) | message.data[7];
            if (message.command == Cmd::Ping && message.response && message.length == 8 &&
                (device == Device::Gleisbox || device == Device::GleisboxNew))
            {
                if (high != nullptr)
                    *high = message.data[4];
                if (low != nullptr)
                    *low = message.data[5];
                return true;
            }
            dispatch(message);
        }
        yield();
    }
    return false;
}

/* -------------------------------------------------------------------
   TrackController::handleUserCommands
-------------------------------------------------------------------  */

void TrackController::handleUserCommands(String command)
{
    CommandParser parser(*this);
    parser.execute(command.c_str(), mLog);
}
