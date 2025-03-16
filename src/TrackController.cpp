
/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
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

#if defined ARDUINO_ARCH_ESP32
#include <ACAN_ESP32.h> // https://github.com/pierremolinaro/acan-esp32.git
#include <Arduino.h>
#elif defined ARDUINO_ARCH_AVR
#include <ACAN2515.h> // https://github.com/pierremolinaro/acan2515.git
static const uint32_t QUARTZ_FREQUENCY = 16UL * 1000UL * 1000UL; // 16 MHz
static const byte MCP2515_INT = 2;                               // INT output of MCP2515 (adapt to your design)
static const byte MCP2515_CS = 10;                               // CS input of MCP2515 (adapt to your design)
ACAN2515 can(MCP2515_CS, SPI, MCP2515_INT);
#endif

static const uint32_t DESIRED_BIT_RATE = 250UL * 1000UL; // Marklin CAN baudrate = 250Kbit/s

/* -------------------------------------------------------------------
   TrackController (constructor / destructor)
-------------------------------------------------------------------  */

TrackController::TrackController()
    : mHash(0),
      mDebug(false),
      mLoopback(false),
      mTimeout(1000)
{
    if (mDebug)
        Serial.println("### Creating controller");
}
TrackController::TrackController(uint64_t timeOut)
    : mHash(0),
      mDebug(false),
      mLoopback(false),
      mTimeout(timeOut)
{
    if (mDebug)
        Serial.println("### Creating controller");
}

TrackController::TrackController(uint16_t hash, bool debug, uint64_t timeOut)
    : mHash(hash),
      mDebug(debug),
      mLoopback(false),
      mTimeout(timeOut)
{
    if (mDebug)
        Serial.println("### Creating controller with param");
}

TrackController::TrackController(uint16_t hash, bool debug, uint64_t timeOut, bool loopback)
    : mHash(hash),
      mDebug(debug),
      mLoopback(loopback),
      mTimeout(timeOut)
{
    if (mDebug)
        Serial.println("### Creating controller with param");
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

void TrackController::init(uint16_t hash, bool debug, bool loopback, uint64_t timeOut)
{
    mHash = hash;
    mDebug = debug;
    mLoopback = loopback;
    mTimeout = timeOut;
}

/* -------------------------------------------------------------------
   TrackController::getHash
-------------------------------------------------------------------  */

uint16_t TrackController::getHash()
{
    return mHash;
}

/* -------------------------------------------------------------------
   TrackController::isDebug
-------------------------------------------------------------------  */

bool TrackController::isDebug()
{
    return mDebug;
}

/* -------------------------------------------------------------------
   TrackController::isLoopback
-------------------------------------------------------------------  */

bool TrackController::isLoopback()
{
    return mLoopback;
}

/* -------------------------------------------------------------------
   TrackController::begin
-------------------------------------------------------------------  */

void TrackController::begin(const byte can_rx_pin, const byte can_tx_pin)
{
    //--- Configure CAN

#if defined ARDUINO_ARCH_ESP32
    Serial.println("Configure ESP32 CAN");
    ACAN_ESP32_Settings settings(DESIRED_BIT_RATE); // Marklin CAN baudrate = 250Kbit/s
    settings.mRxPin = (gpio_num_t)can_rx_pin;
    settings.mTxPin = (gpio_num_t)can_tx_pin;
    const uint32_t errorCode = ACAN_ESP32::can.begin(settings);
#elif defined ARDUINO_ARCH_AVR
    //--- Begin SPI
    SPI.begin();
    Serial.println("Configure ACAN2515");
    ACAN2515Settings settings(QUARTZ_FREQUENCY, DESIRED_BIT_RATE);
    const uint16_t errorCode = can.begin(settings, []
                                         { can.isr(); });
#endif

    if (errorCode)
    {
        Serial.print("Configuration error 0x");
        Serial.println(errorCode, HEX);
    }
    else
    {
        Serial.print("Bit Rate prescaler: ");
        Serial.println(settings.mBitRatePrescaler);
        Serial.print("Triple Sampling: ");
        Serial.println(settings.mTripleSampling ? "yes" : "no");
        Serial.print("Actual bit rate: ");
        Serial.print(settings.actualBitRate());
        Serial.println(" bit/s");
        Serial.print("Exact bit rate ? ");
        Serial.println(settings.exactBitRate() ? "yes" : "no");
        Serial.print("Sample point: ");
        Serial.print(settings.samplePointFromBitStart());
        Serial.println("%");
    }
    Serial.println("Configuration CAN OK");
    Serial.println("");

    delay(500);

    if (!mLoopback)
    {
        TrackMessage message;
        message.clear();
        message.prio = 0x00;
        message.command = 0x1B;
        message.response = false;
        message.length = 5;
        message.data[4] = 0x11;
        sendMessage(message);
    }

    if (mHash == 0)
        generateHash();
}

/* -------------------------------------------------------------------
   TrackController::end
-------------------------------------------------------------------  */

// void TrackController::end() {
// 	detachInterrupt(CAN_INT);

// 	can_t t;

// 	boolean b = dequeue(&t);
// 	while (b) {
// 		b = dequeue(&t);
// 	}
// }

/* -------------------------------------------------------------------
   TrackController::sendMessage
-------------------------------------------------------------------  */

bool TrackController::sendMessage(TrackMessage &message)
{
    CANMessage frame;

    message.hash = mHash;
    frame.id = (static_cast<uint32_t>(message.prio) << 25) | (static_cast<uint32_t>(message.command) << 17) | (static_cast<uint32_t>(message.response) << 16) | message.hash;
    frame.ext = true;
    frame.len = message.length;
    for (byte i = 0; i < message.length; i++)
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
#elif defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_MEGA2560)
    bool result = can.receive(frame);
#endif

    if (result)
    {
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

        message.clear();
        message.command = (frame.id >> 17) & 0xFF;
        message.hash = frame.id & 0xFFFF;
        message.response = (frame.id >> 16) & 0x01;
        message.length = frame.len;

        for (uint8_t i = 0; i < frame.len; i++)
            message.data[i] = frame.data[i];

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

bool TrackController::exchangeMessage(TrackMessage &out, TrackMessage &in, uint16_t timeout)
{

    uint16_t command = out.command;

    if (!sendMessage(out))
    {
        if (mDebug)
        {
            Serial.println(F("!!! Send error"));
            Serial.println(F("!!! Emergency stop"));
            setPower(false);
            for (;;)
                ;
        }
    }

    // uint32_t time = millis();
    uint64_t time = millis();

    /* -- TrackMessage response -- */

    while (millis() < time + timeout)
    {
        in.clear();
        boolean result = receiveMessage(in);

        if (result && in.command == command && in.response)
            return true;
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

bool TrackController::setLocoDirection(const uint16_t address, byte direction)
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

bool TrackController::getLocoDirection(const uint16_t address, byte *direction)
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

bool TrackController::setLocoFunction(const uint16_t address, byte function, byte power)
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

bool TrackController::readConfig(const uint16_t address, uint16_t number, byte *value)
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

    if (exchangeMessage(message, message, mTimeout))
    {
        *value = message.data[6];
        return true;
    }
    else
        return false;
}

/* -------------------------------------------------------------------
   TrackController::getLocoFunction
-------------------------------------------------------------------  */

bool TrackController::getLocoFunction(const uint16_t address, byte function, byte *power)
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
bool TrackController::toggleLocoFunction(const uint16_t address, byte function)
{
    byte power;
    if (getLocoFunction(address, function, &power))
    {
        return setLocoFunction(address, function, power ? 0 : 1);
    }
    return false;
}

/* -------------------------------------------------------------------
   TrackController::setAccessory
-------------------------------------------------------------------  */
boolean TrackController::setAccessory(const uint16_t address, byte position, byte power, uint16_t time)
{
    TrackMessage message;

    message.clear();
    message.command = 0x0B;
    message.length = 6;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = position;
    message.data[5] = power;

    exchangeMessage(message, message, mTimeout);

    if (time != 0)
    {
        delay(time);

        message.clear();
        message.command = 0x0B;
        message.length = 6;
        message.data[2] = (address & 0xFF00) >> 8;
        message.data[3] = (address & 0x00FF);
        message.data[4] = position;

        exchangeMessage(message, message, mTimeout);
    }
    return true;
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
bool TrackController::getAccessory(const uint16_t address, byte *position, byte *power)
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
bool TrackController::getVersion()
{
    bool result = false;

    TrackMessage message;

    message.clear();
    message.command = 0x18;

    if (exchangeMessage(message, message, mTimeout))
    {
        Serial.print("version : ");
        Serial.print(message.data[4]);
        Serial.println(message.data[5]);
        return result = true;
    }

    // sendMessage(message);

    // delay(500);
    // message.clear();
    // while (receiveMessage(message)) {
    //   if (message.command == 0x18 && message.response) {
    //     //if (message.command == 0x18 && message.data[6] == 0x00 && message.data[7] == 0x10) {
    //     // *high = message.data[4];
    //     // *low = message.data[5];
    //     Serial.print("version : ");
    //     Serial.print(message.data[4]);
    //     Serial.println(message.data[5]);
    //     result = true;
    //   }
    // }
    return result;
}

/* -------------------------------------------------------------------
   TrackController::writeConfig
-------------------------------------------------------------------  */

bool TrackController::writeConfig(const uint16_t address, uint16_t number, byte value)
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

    return exchangeMessage(message, message, mTimeout);
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
        uint8_t position = command.substring(16, 17).toInt();
        uint8_t power = command.substring(17, 18).toInt();
        uint16_t time = command.substring(19).toInt();
        setAccessory(address, position, power, time);
    }
    //}
}
