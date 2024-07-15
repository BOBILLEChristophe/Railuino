
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
#include <ACAN_ESP32.h> // https://github.com/pierremolinaro/acan-esp32.git
#include <Arduino.h>

static const uint32_t DESIRED_BIT_RATE = 250UL * 1000UL; // Marklin CAN baudrate = 250Kbit/s

/* -------------------------------------------------------------------
   TrackController (constructor / destructor)
-------------------------------------------------------------------  */

TrackController::TrackController() : mHash(0),
                                     mDebug(false),
                                     mLoopback(false)
{
    if (mDebug)
        Serial.println("### Creating controller");
}

TrackController::TrackController(uint16_t hash, bool debug) : mHash(hash),
                                                              mDebug(debug),
                                                              mLoopback(false)
{
    if (mDebug)
        Serial.println("### Creating controller with param");
}

TrackController::TrackController(uint16_t hash, bool debug, bool loopback) : mHash(hash),
                                                                             mDebug(debug),
                                                                             mLoopback(loopback)
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

// void TrackController::init(uint16_t hash, bool debug, bool loopback)
// {
//     mHash = hash;
//     mDebug = debug;
//     mLoopback = loopback;
// }

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

void TrackController::begin()
{
    //--- Configure ESP32 CAN
    Serial.println("Configure ESP32 CAN");
    ACAN_ESP32_Settings settings(DESIRED_BIT_RATE);
    settings.mRxPin = GPIO_NUM_22; // Optional, default Tx pin is GPIO_NUM_4
    settings.mTxPin = GPIO_NUM_23; // Optional, default Rx pin is GPIO_NUM_5
    const uint32_t errorCode = ACAN_ESP32::can.begin(settings);

    if (errorCode)
    {
        Serial.print("Configuration error 0x");
        Serial.println(errorCode, HEX);
    }
    else
        Serial.println("Configuration CAN OK");
    Serial.println("");

    delay(500);

    if (!mLoopback)
    {
        TrackMessage message;
        message.clear();
        message.priority = 0x00;
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
    CANMessage can;

    message.hash = mHash;
    can.id = (message.priority << 25) | (message.command << 17) | (message.response << 16) | message.hash;
    can.ext = true;
    can.len = message.length;
    for (byte i = 0; i < message.length; i++)
        can.data[i] = message.data[i];

    if (mDebug)
    {
        Serial.print("==> 0x");
        Serial.println(can.id, HEX);
    }
    return ACAN_ESP32::can.tryToSend(can);
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
        mHash = random(0x10000) & 0xFF7F | 0x0300;

        if (mDebug)
        {
            Serial.print(F("### Trying new hash "));
            // TrackMessage::printHex(Serial, mHash, 4);
            Serial.println();
        }

        message.clear();
        message.command = 0x18; // Ping, demande aux equipements sur le bus

        sendMessage(message);

        delay(500);

        ok = true;
        while (receiveMessage(message))
        {
            if (message.hash == mHash)
                ok = false;
        }
    }

    if (mDebug)
        Serial.println(F("### New hash looks good"));
}

/* -------------------------------------------------------------------
   TrackController::receiveMessage
-------------------------------------------------------------------  */

bool TrackController::receiveMessage(TrackMessage &message)
{

    CANMessage can;

    bool result = ACAN_ESP32::can.receive(can);

    if (result)
    {
        if (mDebug)
        {

            Serial.print("ID :");
            Serial.println(can.id, HEX);
            Serial.print("EXIDE:");
            Serial.println(can.ext, HEX);
            Serial.print("DLC:");
            Serial.println(can.len, HEX);
            Serial.print("DATA:");
            // for (int i = 0; i < can.len; i++)
            //     TrackMessage::printHex(Serial, can.data[i], 1);
            Serial.println();
        }

        message.clear();
        message.command = (can.id >> 17) & 0xFF;
        message.hash = can.id & 0xFFFF;
        message.response = (can.id >> 16) & 0x01;
        // message.response = bitRead(can.id, 16) || mLoopback;
        message.length = can.len;

        for (int i = 0; i < can.len; i++)
            message.data[i] = can.data[i];

        if (mDebug)
        {
            Serial.print("<== 0x");
            Serial.println(can.id, HEX);
        }
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

    uint32_t time = millis();

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
   TrackController::setPower
-------------------------------------------------------------------  */

bool TrackController::setPower(bool power)
{
    TrackMessage message;

    if (power)
    {
        /*
          Réinitialiser le compteur de réenregistrement MFX
          Commande système (0x00, dans CAN-ID : 0x00)
          Sous-commande : Compteur de réenregistrement (0x09)
        */
        message.clear();
        message.command = 0x00;   // Commande système (0x00, dans CAN-ID : 0x00)
        message.response = false; // bit de réponse desactivé.
        message.length = 7;
        message.data[4] = 0x09; // Sous-commande Compteur de réenregistrement
        message.data[6] = 0x0D; // Réinitialiser le compteur de réenregistrement à 13.

        /* old version */
        // exchangeMessage(message, message, 1000);

        /* new version */
        if (!exchangeMessage(message, message, 1000))
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
        message.command = 0x00;   // Commande système (0x00, dans CAN-ID : 0x00)
        message.response = false; // bit de réponse desactivé.
        message.length = 6;
        message.data[4] = 0x08; // Sous-commande protocole de voie
        message.data[5] = 0x07; // bit0 = MM2 - bit1 = MFX - bit2 = DCC

        /* old version */
        // exchangeMessage(message, message, 1000);

        /* new version */
        if (!exchangeMessage(message, message, 1000))
        {
            if (mDebug)
                Serial.println("Failed to activate track protocol");
            return false;
        }
    }

    /*
      Arrêt du système ou Démarrage du système
      Commande système (0x00, dans CAN-ID : 0x00)
      Sous-commande dans data[4] = 0: Arrêt  du système (0x01)
      Sous-commande dans data[4] = 1: Démarrage du système (0x01)
    */
    message.clear();
    message.command = 0x00;   // Commande système (0x00, dans CAN-ID : 0x00)
    message.response = false; // bit de réponse desactivé.
    message.length = 5;
    message.data[4] = power ? true : false; // Sous-commande Arrêt ou Démarrage

    return exchangeMessage(message, message, 1000);
}

/* -------------------------------------------------------------------
   TrackController::setLocoDirection
-------------------------------------------------------------------  */

bool TrackController::setLocoDirection(const uint16_t address, byte direction)
{
    TrackMessage message;

    /* Sur la MS2, le changement de direction est pédédé d'un arret d'urgence de la locomotive
       Commande systeme 0x00 Sous Commande 0x03

       Cet arret d'urgence est remplace ici par une vitesse 0
    */
    // message.clear();
    // message.command = 0x00;
    // message.length = 5;
    // message.data[2] = (address & 0xFF00) >> 8;
    // message.data[3] = (address & 0x00FF);
    // message.data[4] = 0x03;

    message.clear();
    message.command = 0x04;
    message.length = 6;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[5] = 0;

    exchangeMessage(message, message, 1000);

    message.clear();
    message.command = 0x05;
    message.length = 5;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = direction;

    return exchangeMessage(message, message, 1000);
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

    if (exchangeMessage(message, message, 1000))
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

    return exchangeMessage(message, message, 1000);
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

    if (exchangeMessage(message, message, 10000))
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

    if (exchangeMessage(message, message, 1000))
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

    return exchangeMessage(message, message, 1000);
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

    exchangeMessage(message, message, 1000);

    if (time != 0)
    {
        delay(time);

        message.clear();
        message.command = 0x0B;
        message.length = 6;
        message.data[2] = (address & 0xFF00) >> 8;
        message.data[3] = (address & 0x00FF);
        message.data[4] = position;

        exchangeMessage(message, message, 1000);
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

    if (exchangeMessage(message, message, 1000))
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

    if (exchangeMessage(message, message, 1000))
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
bool TrackController::getVersion(byte *high, byte *low)
{
    bool result = false;

    TrackMessage message;

    message.clear();
    message.command = 0x18;
    sendMessage(message);

    delay(500);

    while (receiveMessage(message))
    {
        if (message.command = 0x18 && message.data[6] == 0x00 && message.data[7] == 0x10)
        {
            *high = message.data[4];
            *low = message.data[5];
            result = true;
        }
    }
    return result;
}

/* -------------------------------------------------------------------
   TrackController::writeConfig
-------------------------------------------------------------------  */

bool TrackController::writeConfig(const uint16_t address, uint16_t number, byte value)
{
    TrackMessage message;

    message.clear();
    message.priority = 0x01;
    message.command = 0x08;
    message.length = 8;
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = highByte(number);
    message.data[5] = lowByte(number);
    message.data[6] = value;

    return exchangeMessage(message, message, 10000);
}

/* -------------------------------------------------------------------
   TrackController::handleUserCommands
-------------------------------------------------------------------  */

void TrackController::handleUserCommands(String command)
{
    if (command.startsWith("power "))
    {
        bool power = command.substring(6).toInt();
        setPower(power);
    }
    else if (command.startsWith("direction "))
    {
        uint16_t address = command.substring(10, 15).toInt();
        uint8_t direction = command.substring(16).toInt();
        setLocoDirection(address, direction);
    }
    else if (command.startsWith("speed "))
    {
        uint16_t address = command.substring(6, 11).toInt();
        uint16_t speed = command.substring(12).toInt();
        setLocoSpeed(address, speed);
    }
    else if (command.startsWith("function "))
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
