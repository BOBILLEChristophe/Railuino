
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
 * Copyright (C) 2026 Uwe Monreal
 *
 * This example is free software; you can redistribute it and/or
 * modify it under the terms of the Creative Commons Zero License,
 * version 1.0, as published by the Creative Commons Organisation.
 * This effectively puts the file into the public domain.
 *
 * This example is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#include "TrackController.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <ACAN_ESP32.h> // https://github.com/pierremolinaro/acan-esp32.git

#elif defined(ARDUINO_ARCH_AVR)
#include <ACAN2515.h> // https://github.com/pierremolinaro/acan2515.git

// MCP2515 wiring and quartz. The defaults match the usual Uno/Mega
// shields; override them without touching the library, for instance in
// platformio.ini:
//   build_flags = -D RAILUINO_MCP2515_QUARTZ=8000000UL -D RAILUINO_MCP2515_CS=53
// Many cheap MCP2515 modules carry an 8 MHz quartz: with the wrong value
// the bit rate is wrong and nothing is received.
#ifndef RAILUINO_MCP2515_QUARTZ
#define RAILUINO_MCP2515_QUARTZ 16000000UL // 16 MHz
#endif
#ifndef RAILUINO_MCP2515_CS
#define RAILUINO_MCP2515_CS 10 // CS input of MCP2515
#endif
#ifndef RAILUINO_MCP2515_INT
#define RAILUINO_MCP2515_INT 2 // INT output of MCP2515
#endif

static const uint32_t QUARTZ_FREQUENCY = RAILUINO_MCP2515_QUARTZ;
static const uint8_t MCP2515_CS = RAILUINO_MCP2515_CS;
static const uint8_t MCP2515_INT = RAILUINO_MCP2515_INT;
static ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);

#else
#error "Railuino supports ESP32 (TWAI) and AVR boards with an MCP2515 (Uno, Mega...)"
#endif

static const uint32_t DESIRED_BIT_RATE = 250UL * 1000UL; // Marklin CAN baudrate = 250Kbit/s

/* -------------------------------------------------------------------
   TrackController (constructor / destructor)
-------------------------------------------------------------------  */

// Out-of-class definitions, required in C++11 if the constants are ODR-used.
constexpr uint32_t TrackController::DEFAULT_TIMEOUT;
constexpr uint32_t TrackController::CV_TIMEOUT;

TrackController::TrackController()
    : mHash(0),
      mDebug(false),
      mLoopback(false),
      mTimeout(DEFAULT_TIMEOUT)
{
}

// Note: no Serial output here. Global objects are constructed before
// setup() runs, i.e. before Serial.begin() has been called.
TrackController::TrackController(uint16_t hash, bool debug, uint32_t timeOut, bool loopback)
    : mHash(hash),
      mDebug(debug),
      mLoopback(loopback),
      mTimeout(timeOut)
{
}

TrackController::~TrackController() // Destructeur
{
    if (mDebug)
        Serial.println("### Destroying controller");
}

/* -------------------------------------------------------------------
   TrackController::init
-------------------------------------------------------------------  */

// Kept for compatibility with older versions but not used from v-0.9.x

void TrackController::init(uint16_t hash, bool debug, bool loopback, uint32_t timeOut)
{
    mHash = hash;
    mDebug = debug;
    mLoopback = loopback;
    mTimeout = timeOut;
}

/* -------------------------------------------------------------------
   TrackController::setTimeout / getTimeout
-------------------------------------------------------------------  */

void TrackController::setTimeout(uint32_t timeOut)
{
    mTimeout = timeOut;
}

uint32_t TrackController::getTimeout() const
{
    return mTimeout;
}

/* -------------------------------------------------------------------
   TrackController::getHash
-------------------------------------------------------------------  */

uint16_t TrackController::getHash() const
{
    return mHash;
}

/* -------------------------------------------------------------------
   TrackController::isDebug
-------------------------------------------------------------------  */

bool TrackController::isDebug() const
{
    return mDebug;
}

/* -------------------------------------------------------------------
   TrackController::isLoopback
-------------------------------------------------------------------  */

bool TrackController::isLoopback() const
{
    return mLoopback;
}

/* -------------------------------------------------------------------
   TrackController::begin
-------------------------------------------------------------------  */

bool TrackController::begin(const uint8_t can_rx_pin, const uint8_t can_tx_pin)
{
    //--- Configure CAN

#if defined(ARDUINO_ARCH_ESP32)
    ACAN_ESP32_Settings settings(DESIRED_BIT_RATE); // Marklin CAN baudrate = 250Kbit/s
    settings.mRxPin = static_cast<gpio_num_t>(can_rx_pin);
    settings.mTxPin = static_cast<gpio_num_t>(can_tx_pin);
    const uint32_t errorCode = ACAN_ESP32::can.begin(settings);
#elif defined(ARDUINO_ARCH_AVR)
    (void)can_rx_pin; // pins are fixed by the SPI bus and RAILUINO_MCP2515_*
    (void)can_tx_pin;
    SPI.begin();
    ACAN2515Settings settings(QUARTZ_FREQUENCY, DESIRED_BIT_RATE);
    const uint16_t errorCode = can.begin(settings, []
                                         { can.isr(); });
#endif

    if (errorCode)
    {
        // Always reported: without CAN nothing else can work.
        Serial.print(F("!!! CAN configuration error 0x"));
        Serial.println(errorCode, HEX);
        return false;
    }

    if (mDebug)
    {
        Serial.print(F("Bit Rate prescaler: "));
        Serial.println(settings.mBitRatePrescaler);
        Serial.print(F("Triple Sampling: "));
        Serial.println(settings.mTripleSampling ? F("yes") : F("no"));
        Serial.print(F("Actual bit rate: "));
        Serial.print(settings.actualBitRate());
        Serial.println(F(" bit/s"));
        Serial.print(F("Exact bit rate ? "));
        Serial.println(settings.exactBitRate() ? F("yes") : F("no"));
        Serial.print(F("Sample point: "));
        Serial.print(settings.samplePointFromBitStart());
        Serial.println(F("%"));
        Serial.println(F("Configuration CAN OK"));
    }

    delay(500);

    bool ok = true;

    if (!mLoopback)
    {
        // Wakes up the Gleisbox (60112/60113). Sent once: exchangeMessage()
        // sends the message itself (it used to be sent a second time
        // with sendMessage() just before).
        TrackMessage message;
        message.clear();
        message.prio = 0x00;
        message.command = 0x1B;
        message.response = false;
        message.length = 5;
        message.data[4] = 0x11;

        ok = exchangeMessage(message, message, mTimeout);
        if (mDebug)
            Serial.println(ok ? F("Gleisbox OK (0x1B 0x11)") : F("!!! No answer from Gleisbox (0x1B 0x11)"));
    }

    if (mHash == 0)
        generateHash();

    return ok;
}

/* -------------------------------------------------------------------
   TrackController::end
-------------------------------------------------------------------  */

void TrackController::end()
{
    // Discards the messages waiting in the CAN driver's receive FIFO.
    TrackMessage message;
    while (receiveMessage(message))
        ;
}

/* -------------------------------------------------------------------
   TrackController::sendMessage
-------------------------------------------------------------------  */

bool TrackController::sendMessage(TrackMessage &message)
{
    CANMessage frame;

    if (message.length > 8)
        return false;

    message.hash = mHash;
    frame.id = (static_cast<uint32_t>(message.prio) << 25) | (static_cast<uint32_t>(message.command) << 17) | (static_cast<uint32_t>(message.response) << 16) | message.hash;
    frame.ext = true;
    frame.len = message.length;
    for (uint8_t i = 0; i < message.length; i++)
        frame.data[i] = message.data[i];

    if (mDebug)
    {
        Serial.print("<== ID : 0x");
        Serial.println(frame.id, HEX);
        Serial.print("EXT : ");
        Serial.println(frame.ext ? "extended" : "standard");
        Serial.print("RESP : ");
        Serial.println((frame.id & 0x10000) >> 16);
        Serial.print("DLC : ");
        Serial.println(frame.len);
        Serial.print("COMMAND : 0x");
        TrackMessage::printHex(Serial, (frame.id & 0x1FE0000) >> 17, 2);
        Serial.print("\nDATA : ");
        for (uint8_t i = 0; i < frame.len; i++)
        {
            Serial.print("0x");
            TrackMessage::printHex(Serial, frame.data[i], 2);
            if (i < frame.len - 1)
                Serial.print(" - ");
        }
        Serial.print("\n------------------------------------------------------------------\n");
    }

#if defined(ARDUINO_ARCH_ESP32)
    return ACAN_ESP32::can.tryToSend(frame);
#elif defined(ARDUINO_ARCH_AVR)
    return can.tryToSend(frame);
#endif
}

/* -------------------------------------------------------------------
   TrackController::receiveMessage
-------------------------------------------------------------------  */

bool TrackController::receiveMessage(TrackMessage &message)
{

    CANMessage frame;

#if defined(ARDUINO_ARCH_ESP32)
    bool result = ACAN_ESP32::can.receive(frame);
#elif defined(ARDUINO_ARCH_AVR)
    bool result = can.receive(frame);
#endif

    if (result)
    {
        message.clear();
        message.prio = (frame.id >> 25) & 0x0F;
        message.command = (frame.id >> 17) & 0xFF;
        message.hash = frame.id & 0xFFFF;
        message.response = (frame.id >> 16) & 0x01;
        message.length = frame.len;

        for (uint8_t i = 0; i < frame.len; i++)
            message.data[i] = frame.data[i];

        if (mDebug)
        {
            Serial.print("==> ID : 0x");
            Serial.println(frame.id, HEX);
            Serial.print("EXT : ");
            Serial.println(frame.ext ? "extended" : "standard");
            Serial.print("RESP : ");
            Serial.println((frame.id & 0x10000) >> 16);
            Serial.print("DLC : ");
            Serial.println(frame.len);
            Serial.print("COMMAND : 0x");
            TrackMessage::printHex(Serial, (frame.id & 0x1FE0000) >> 17, 2);
            Serial.print("\nDATA : ");
            for (uint8_t i = 0; i < frame.len; i++)
            {
                Serial.print("0x");
                TrackMessage::printHex(Serial, frame.data[i], 2);
                if (i < frame.len - 1)
                    Serial.print(" - ");
            }
            Serial.print("\n------------------------------------------------------------------\n");
        }

        // if (mDebug) {
        //   Serial.print("<== 0x");
        //   Serial.println(frame.id, HEX);
        // }
    }
    return result;
}

/* -------------------------------------------------------------------
   TrackController::exchangeMessage
-------------------------------------------------------------------  */

bool TrackController::isResponseTo(const TrackMessage &out, const TrackMessage &in)
{
    if (!in.response || in.command != out.command)
        return false;

    // Same filter as before: ignore responses carrying 4 bytes or less.
    if (in.length <= 4)
        return false;

    switch (out.command)
    {
    case 0x00: // System command: UID in data[0..3], sub-command in data[4]
        if (out.length >= 5 && in.data[4] != out.data[4])
            return false;
        // fall through
    case 0x04: // Speed
    case 0x05: // Direction
    case 0x06: // Function
    case 0x07: // Read Config
    case 0x08: // Write Config
    case 0x0B: // Accessory
        // The response must concern the same locomotive / accessory.
        // Without this check, the answer for another locomotive (sent
        // by an MS2 on the same bus) could be taken as ours.
        for (uint8_t i = 0; i < 4; i++)
            if (in.data[i] != out.data[i])
                return false;
        return true;

    default: // Ping (0x18), Discovery (0x01), Bootloader (0x1B)...
        return true;
    }
}

bool TrackController::exchangeMessage(TrackMessage &out, TrackMessage &in, uint32_t timeout)
{
    // Keep a copy of the request: 'out' and 'in' may be the same object.
    const TrackMessage request = out;

    if (!sendMessage(out))
    {
        // Previously: setPower(false) then for(;;) in debug mode, which
        // froze the controller. The caller now decides what to do.
        if (mDebug)
            Serial.println(F("!!! Send error"));
        return false;
    }

    // Unsigned subtraction: stays correct when millis() wraps around
    // (every 49.7 days). "millis() < start + timeout" did not.
    const uint32_t start = millis();

    while (millis() - start < timeout)
    {
        in.clear();
        const bool received = receiveMessage(in);

        if (received && mDebug)
        {
            Serial.println(F("=== TRAME CAN RECUE ==="));
            in.printTo(Serial);
            Serial.println();
        }

        if (received && isResponseTo(request, in))
            return true;
        yield();
    }

    if (mDebug)
        Serial.println(F("!!! Receive timeout"));

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
        mHash = (random(0x10000) & 0xFF7F) | 0x0300;

        if (mDebug)
        {
            Serial.print(F("### Trying new hash 0x"));
            message.printHex(Serial, mHash, 4);
            Serial.print(F("\n------------------------------------------------------------------\n"));
        }

        message.clear();
        message.command = 0x18; // Ping, demande aux equipements sur le bus

        sendMessage(message);

        delay(mTimeout);

        ok = true;
        while (receiveMessage(message))
        {
            if (message.hash == mHash)
                ok = false;
        }
    }

    if (mDebug)
    {
        Serial.print(F("### New hash looks good"));
        Serial.print(F("\n------------------------------------------------------------------\n"));
    }
}

/* -------------------------------------------------------------------
   TrackController::locomotive Discovery
-------------------------------------------------------------------  */

bool TrackController::locoDiscovery()
{
    TrackMessage message;
    Serial.print("###Start discovery");
    Serial.print(F("\n------------------------------------------------------------------\n"));

    // 2.1 Commande : Système Arrêt
    // message.clear();
    // message.prio = 0x00;
    // message.command = 0x00;
    // message.length = 5;
    // message.data[4] = 0x00; // Arrêt du système
    // sendMessage(message);
    // delay(1000);

    // Bootloader CAN
    message.clear();
    message.prio = 0x00;
    message.command = 0x1B;
    message.length = 5;
    message.data[4] = 0x11;
    sendMessage(message);
    delay(1000);

    // 2.10 Commande : Système MFX Définir le compteur de nouvelles inscriptions
    message.clear();
    message.prio = 0x00;
    message.command = 0x00;
    message.length = 7;
    message.data[4] = 0x09;
    message.data[6] = 0x07;
    sendMessage(message);
    delay(1000);

    // 3.3 Commande : MFX Verify (pour provoquer un UnBIND)
    message.clear();
    message.prio = 0x00;
    message.command = 0x03;
    message.length = 6;
    sendMessage(message);
    delay(1000);

    // 2.1 Commande : Système Go
    message.clear();
    message.prio = 0x00;
    message.command = 0x00;
    message.length = 5;
    message.data[4] = 0x01; // Système Go
    sendMessage(message);
    delay(1000);

    // 3.1 Ordre de mission : Locomotive Discovery
    // message.clear();
    // message.prio = 0x00;
    // message.command = 0x00;
    // message.length = 1;
    // message.data[0] = 0x20; // MFX protocole;

    message.clear();
    message.prio = 0x00;
    message.command = 0x01;
    message.length = 5;
    message.data[0] = 0xF9; // MFX protocole;
    message.data[1] = 0xE6; // MFX protocole;
    message.data[2] = 0x1;  // MFX protocole;
    message.data[3] = 0x20; // MFX protocole;
    message.data[4] = 0x20; // MFX protocole;

    // sendMessage(message);

    if (exchangeMessage(message, message, 60UL * 1000UL))
    {
        Serial.print("UID : 0x");
        message.printHex(Serial, message.data[0], 2);
        message.printHex(Serial, message.data[1], 2);
        message.printHex(Serial, message.data[2], 2);
        message.printHex(Serial, message.data[3], 2);
        return true;
    }
    else
        return false;
}

/* -------------------------------------------------------------------
   TrackController::setPower
-------------------------------------------------------------------  */

bool TrackController::setPower(bool power)
{

    TrackMessage message;

    auto exchange = [this, &message](uint16_t timeout)
    {
        return exchangeMessage(message, message, timeout);
    };

    /*
        Arrêt du système ou Démarrage du système
        Commande système (0x00, dans CAN-ID : 0x00)
        Sous-commande dans data[4] = 0: Arrêt  du système (0x00)
        Sous-commande dans data[4] = 1: Démarrage du système (0x01)
      */
    message.clear();
    message.prio = 0x00;
    message.command = 0x00;   // Commande système (0x00, dans CAN-ID : 0x00)
    message.response = false; // bit de réponse désactivé.
    message.length = 5;
    message.data[4] = power ? true : false; // Sous-commande Arrêt ou Démarrage

    // /* new version */
    if (!exchange(mTimeout))
    {
        if (mDebug)
        {
            Serial.println("Failed to send Power");
        }
        return false;
    }
    else
    {
        if (mDebug)
        {
            Serial.print("Power ");
            Serial.print(power ? "on" : "off");
            Serial.print("\n------------------------------------------------------------------\n");
        }
    }

    if (power)
    {
        /*
              Réinitialiser le compteur de réenregistrement MFX
              Commande système (0x00, dans CAN-ID : 0x00)
              Sous-commande : Compteur de réenregistrement (0x09)
        */
        message.clear();
        message.prio = 0x00;
        message.command = 0x00;   // Commande système (0x00, dans CAN-ID : 0x00)
        message.response = false; // bit de réponse desactivé.
        message.length = 7;
        message.data[4] = 0x09; // Sous-commande Compteur de réenregistrement
        message.data[6] = 0x03; // Réinitialiser le compteur de réenregistrement à 3.

        /* old version */
        // exchangeMessage(message, message, 1000);

        /* new version */
        if (!exchange(mTimeout))
        {
            if (mDebug)
                Serial.println("Failed to reset re-registration counter");
            return false;
        }

        /*
             Activer ou désactiver le protocole de voie
             Commande système (0x00, dans CAN-ID : 0x00)
             Sous-commande : •	Protocole de voie (0x08)
           */
        message.clear();
        message.prio = 0x00;
        message.command = 0x00;   // Commande système (0x00, dans CAN-ID : 0x00)
        message.response = false; // bit de réponse desactivé.
        message.length = 6;
        message.data[4] = 0x08; // Sous-commande protocole de voie
        message.data[5] = 0x07; // bit0 = MM2 - bit1 = MFX - bit2 = DCC

        /* old version */
        // exchangeMessage(message, message, 1000);

        /* new version */
        if (!exchange(mTimeout))
        {
            if (mDebug)
                Serial.println("Failed to activate track protocol");
            return false;
        }
    }
    return true;
}

/* -------------------------------------------------------------------
   TrackController::systemHalt
-------------------------------------------------------------------  */

bool TrackController::systemHalt(const uint16_t address)
{
    TrackMessage message;

    message.clear();
    message.prio = 0x00;
    message.command = 0x00;
    message.length = 5;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = 0x02;

    return exchangeMessage(message, message, mTimeout);
}

/* -------------------------------------------------------------------
   TrackController::emergency
-------------------------------------------------------------------  */

bool TrackController::emergency(const uint16_t address)
{
    TrackMessage message;

    message.clear();
    message.prio = 0x00;
    message.command = 0x00;
    message.length = 5;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = 0x03;

    return exchangeMessage(message, message, mTimeout);
}

/* -------------------------------------------------------------------
   TrackController::setLocoDirection
-------------------------------------------------------------------  */

bool TrackController::setLocoDirection(const uint16_t address, uint8_t direction)
{
    TrackMessage message;

    /* Sur la MS2, le changement de direction est precede d'un arret d'urgence de la locomotive
         Commande systeme 0x00 Sous Commande 0x03

         Cet arret d'urgence est remplace ici par une vitesse 0
      */
    // message.clear();
    // message.command = 0x00;
    // message.length = 5;
    // message.data[2] = (address & 0xFF00) >> 8;
    // message.data[3] = (address & 0x00FF);
    // message.data[4] = 0x03;

    // message.clear();
    // message.command = 0x04;  // Commande : LocVitesse
    // message.length = 6;
    // message.data[2] = (address & 0xFF00) >> 8;
    // message.data[3] = (address & 0x00FF);
    // message.data[5] = 0;

    // return exchangeMessage(message, message, mTimeout);

    message.clear();
    message.command = 0x05;
    message.length = 5;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = direction;

    return exchangeMessage(message, message, mTimeout);
}

/* -------------------------------------------------------------------
   TrackController::toggleLocoDirection
-------------------------------------------------------------------  */
bool TrackController::toggleLocoDirection(const uint16_t address)
{
    return setLocoDirection(address, DIR_CHANGE);
}

/* -------------------------------------------------------------------
   TrackController::getLocoDirection
-------------------------------------------------------------------  */

bool TrackController::getLocoDirection(const uint16_t address, uint8_t *direction)
{
    TrackMessage message;

    message.clear();
    message.command = 0x05;
    message.length = 4;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);

    if (exchangeMessage(message, message, mTimeout))
    {
        *direction = message.data[4];
        return true;
    }
    else
        return false;
}

/* -------------------------------------------------------------------
   TrackController::setLocoFunction
-------------------------------------------------------------------  */

bool TrackController::setLocoFunction(const uint16_t address, uint8_t function, uint8_t power)
{
    TrackMessage message;

    message.clear();
    message.command = 0x06;
    message.length = 6;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = function;
    message.data[5] = power;

    return exchangeMessage(message, message, mTimeout);
}

/* -------------------------------------------------------------------
   TrackController::readConfig
-------------------------------------------------------------------  */

bool TrackController::readConfig(const uint16_t address, uint16_t number, uint8_t *value)
{
    TrackMessage message;

    message.clear();
    message.command = 0x07;
    message.length = 7;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = highByte(number);
    message.data[5] = lowByte(number);
    message.data[6] = 0x01;

    if (exchangeMessage(message, message, CV_TIMEOUT))
    {
        value[0] = message.data[6];
        return true;
    }
    else
    {
        return false;
    }
}

/* -------------------------------------------------------------------
   TrackController::getLocoFunction
-------------------------------------------------------------------  */

bool TrackController::getLocoFunction(const uint16_t address, uint8_t function, uint8_t *power)
{
    TrackMessage message;

    message.clear();
    message.command = 0x06;
    message.length = 5;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = function;

    if (exchangeMessage(message, message, mTimeout))
    {
        *power = message.data[5];
        return true;
    }
    else
        return false;
}

/* -------------------------------------------------------------------
   TrackController::setLocoSpeed
-------------------------------------------------------------------  */

bool TrackController::setLocoSpeed(const uint16_t address, uint16_t speed)
{
    TrackMessage message;

    message.clear();
    message.command = 0x04;
    message.length = 6;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = address & 0x00FF;
    message.data[4] = (speed & 0xFF00) >> 8;
    message.data[5] = speed & 0x00FF;

    return exchangeMessage(message, message, mTimeout);
}

/* -------------------------------------------------------------------
   TrackController::toggleLocoFunction
-------------------------------------------------------------------  */
bool TrackController::toggleLocoFunction(const uint16_t address, uint8_t function)
{
    uint8_t power;
    if (getLocoFunction(address, function, &power))
    {
        return setLocoFunction(address, function, power ? 0 : 1);
    }
    return false;
}

/* -------------------------------------------------------------------
   TrackController::setAccessory
-------------------------------------------------------------------  */
bool TrackController::setAccessory(const uint16_t address, uint8_t position, uint8_t power, uint16_t time)
{
    TrackMessage message;

    message.clear();
    message.command = 0x0B;
    message.length = 6;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = position;
    message.data[5] = power;

    bool ok = exchangeMessage(message, message, mTimeout);

    if (time != 0)
    {
        delay(time);

        // Switch the coil off (power = 0). Always sent, even if the first
        // message was not acknowledged, so that a coil is never left on.
        message.clear();
        message.command = 0x0B;
        message.length = 6;
        message.data[2] = (address & 0xFF00) >> 8;
        message.data[3] = (address & 0x00FF);
        message.data[4] = position;

        ok = exchangeMessage(message, message, mTimeout) && ok;
    }
    return ok;
}

/* -------------------------------------------------------------------
   TrackController::setTurnout
-------------------------------------------------------------------  */

bool TrackController::setTurnout(const uint16_t address, bool straight)
{
    return setAccessory(address, straight ? ACC_STRAIGHT : ACC_ROUND, 1, 1000);
}

/* -------------------------------------------------------------------
   TrackController::getLocoSpeed
-------------------------------------------------------------------  */

bool TrackController::getLocoSpeed(const uint16_t address, uint16_t *speed)
{
    TrackMessage message;

    message.clear();
    message.command = 0x04;
    message.length = 4;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);

    if (exchangeMessage(message, message, mTimeout))
    {
        *speed = (message.data[4] << 8) | message.data[5];
        return true;
    }
    else
        return false;
}

/* -------------------------------------------------------------------
   TrackController::getAccessory
-------------------------------------------------------------------  */
bool TrackController::getAccessory(const uint16_t address, uint8_t *position, uint8_t *power)
{
    TrackMessage message;

    message.clear();
    message.command = 0x0B;
    message.length = 4;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);

    if (exchangeMessage(message, message, mTimeout))
    {
        position[0] = message.data[4];
        power[0] = message.data[5];
        return true;
    }
    else
    {
        return false;
    }
}

/* -------------------------------------------------------------------
   TrackController::getVersion
-------------------------------------------------------------------  */
bool TrackController::getVersion(uint8_t *high, uint8_t *low)
{
    // Ping (0x18): every device on the bus answers with its software
    // version in data[4..5] and its device type in data[6..7]. The MS2
    // often answers first, so we keep listening until the Gleisbox
    // (device type 0x0010) has answered or the timeout expires.
    TrackMessage message;
    message.clear();
    message.command = 0x18;

    if (!sendMessage(message))
        return false;

    const uint32_t start = millis();
    while (millis() - start < mTimeout)
    {
        if (receiveMessage(message) && message.command == 0x18 && message.response && message.length == 8 && message.data[6] == 0x00 && message.data[7] == 0x10)
        {
            if (high != nullptr)
                *high = message.data[4];
            if (low != nullptr)
                *low = message.data[5];
            return true;
        }
        yield();
    }
    return false;
}

/* -------------------------------------------------------------------
   TrackController::writeConfig
-------------------------------------------------------------------  */

bool TrackController::writeConfig(const uint16_t address, uint16_t number, uint8_t value)
{
    TrackMessage message;

    message.clear();
    message.prio = 0x01;
    message.command = 0x08;
    message.length = 8;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = highByte(number);
    message.data[5] = lowByte(number);
    message.data[6] = value;

    // CV_TIMEOUT (10 s), as in Railuino 0.9: a write can take seconds.
    return exchangeMessage(message, message, CV_TIMEOUT);
}

/* -------------------------------------------------------------------
   TrackController::handleUserCommands
-------------------------------------------------------------------  */

void TrackController::handleUserCommands(String command)
{
    if (command.startsWith("power ")) // EX power 1 (on); power 0 (off)
    {
        bool power = command.substring(6).toInt();
        setPower(power);
    }
    else if (command.startsWith("emergency "))
    {
        uint16_t address = command.substring(10, 15).toInt();
        emergency(address);
    }
    else if (command.startsWith("direction "))
    {
        uint16_t address = command.substring(10, 15).toInt();
        uint8_t direction = command.substring(16).toInt();
        setLocoDirection(address, direction);
    }
    else if (command.startsWith("speed ")) // Ex speed 16391 100; 16391 = 0x40 | 0x07; 100 = speed/1000
    {
        uint16_t address = command.substring(6, 11).toInt();
        uint16_t speed = command.substring(12).toInt();
        setLocoSpeed(address, speed);
    }
    else if (command.startsWith("function ")) // Ex function 16391 0 1; 16391 = 0x40 | 0x07; 0 = feux; 1 = true
    {
        uint16_t address = command.substring(9, 14).toInt();
        uint8_t function = command.substring(15, 16).toInt();
        uint8_t power = command.substring(16).toInt();
        setLocoFunction(address, function, power);
    }
    else if (command.startsWith("accessory "))
    {
        uint16_t address = command.substring(10, 15).toInt();
        // Format: "accessory AAAAA P W TTT" (address on 5 digits,
        // position, power, time in ms). Power used to be read at
        // index 17, i.e. on the space, and was therefore always 0.
        uint8_t position = command.substring(16, 17).toInt();
        uint8_t power = command.substring(18, 19).toInt();
        uint16_t time = command.substring(20).toInt();
        setAccessory(address, position, power, time);
    }
    //}
}
