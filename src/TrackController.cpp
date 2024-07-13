
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

TrackController::TrackController() // Constructeur
{
    if (mDebug)
        Serial.println("### Creating controller");

    init(0, false, false);
}

TrackController::TrackController(uint16_t hash, bool debug) // Constructeur
{
    if (mDebug)
        Serial.println("### Creating controller with param");

    init(hash, debug, false);
}

TrackController::~TrackController() // Destructeur
{
    if (mDebug)
        Serial.println("### Destroying controller");

    end();
}

/* -------------------------------------------------------------------
   TrackController::init
-------------------------------------------------------------------  */

void TrackController::init(uint16_t hash, bool debug, bool loopback)
{
    mHash = hash;
    mDebug = debug;
    mLoopback = loopback;
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

    // delay(500);

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

void TrackController::end()
{
    // Probablement plus nécessaires
}

/* -------------------------------------------------------------------
   TrackController::sendMessage
-------------------------------------------------------------------  */

bool TrackController::sendMessage(TrackMessage &message)
{
    CANMessage can;

    message.hash = mHash;
    can.id = (message.priority << 25) | (message.command << 18) | (message.response << 17) | message.hash;
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
        message.clear();
        message.command = 0x00;
        message.length = 7;
        message.data[4] = 0x09;
        message.data[6] = 0x0D;

        exchangeMessage(message, message, 1000);

        message.clear();
        message.command = 0x00;
        message.length = 6;
        message.data[4] = 0x08;
        message.data[5] = 0x07;

        exchangeMessage(message, message, 1000);
    }

    message.clear();
    message.command = 0x00;
    message.length = 5;
    message.data[4] = power ? true : false;

    return exchangeMessage(message, message, 1000);
}

/* -------------------------------------------------------------------
   TrackController::setLocoDirection
-------------------------------------------------------------------  */

bool TrackController::setLocoDirection(uint16_t address, byte direction)
{
    TrackMessage message;

    message.clear();
    message.command = 0x00;
    message.length = 5;
    // message.data[2] = highByte(address);
    // message.data[3] = lowByte(address);
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = 0x03;

    exchangeMessage(message, message, 1000);

    message.clear();
    message.command = 0x05;
    message.length = 3;
    // message.data[2] = highByte(address);
    // message.data[3] = lowByte(address);
    message.data[2] = (address & 0xFF00) >> 8;
    message.data[3] = (address & 0x00FF);
    message.data[4] = direction;

    return exchangeMessage(message, message, 1000);
}

/* -------------------------------------------------------------------
   TrackController::setLocoDirection
-------------------------------------------------------------------  */
bool TrackController::toggleLocoDirection(uint16_t address)
{
    return setLocoDirection(address, DIR_CHANGE);
}

/* -------------------------------------------------------------------
   TrackController::getLocoDirection
-------------------------------------------------------------------  */

bool TrackController::getLocoDirection(uint16_t address, byte *direction)
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

bool TrackController::setLocoFunction(uint16_t address, byte function, byte power) {
	TrackMessage message;

	message.clear();
	message.command = 0x06;
	message.length = 0x06;
	message.data[2] = highByte(address);
	message.data[3] = lowByte(address);
	message.data[4] = function;
	message.data[5] = power;

	return exchangeMessage(message, message, 1000);
}

/* -------------------------------------------------------------------
   TrackController::setLocoFunction
-------------------------------------------------------------------  */

bool TrackController::readConfig(uint16_t address, uint16_t number, byte *value) {
	TrackMessage message;

	message.clear();
	message.command = 0x07;
	message.length = 0x07;
	message.data[2] = highByte(address);
	message.data[3] = lowByte(address);
	message.data[4] = highByte(number);
	message.data[5] = lowByte(number);
	message.data[6] = 0x01;

	if (exchangeMessage(message, message, 10000)) {
		*value = message.data[6];
		return true;
	} else {
		return false;
	}
}
