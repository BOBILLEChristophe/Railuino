/*********************************************************************
 * Railuino - Hacking your Märklin
 *
 * Copyright (C) 2012 Joerg Pleumann
 * Copyright (C) 2024 christophe bobille
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

#include "TrackReporterS88.h"

// Half period of the S88 clock, in microseconds.
static const unsigned int TIME = 50;

constexpr uint8_t TrackReporterS88::MAX_MODULES;
constexpr uint8_t TrackReporterS88::DEFAULT_DATA_PIN;
constexpr uint8_t TrackReporterS88::DEFAULT_CLOCK_PIN;
constexpr uint8_t TrackReporterS88::DEFAULT_LOAD_PIN;
constexpr uint8_t TrackReporterS88::DEFAULT_RESET_PIN;

TrackReporterS88::TrackReporterS88(uint8_t modules, uint8_t dataPin, uint8_t clockPin,
								   uint8_t loadPin, uint8_t resetPin)
	: mSize(modules > MAX_MODULES ? MAX_MODULES : modules),
	  mDataPin(dataPin),
	  mClockPin(clockPin),
	  mLoadPin(loadPin),
	  mResetPin(resetPin),
	  mSwitches{}
{
	pinMode(mDataPin, INPUT);
	pinMode(mClockPin, OUTPUT);
	pinMode(mLoadPin, OUTPUT);
	pinMode(mResetPin, OUTPUT);
}

void TrackReporterS88::refresh()
{
	refresh(mSize);
}

void TrackReporterS88::refresh(uint8_t modules)
{
	if (modules > mSize)
		modules = mSize;

	// '<' and not '<=': mSwitches[sizeof(mSwitches)] is out of the array.
	for (uint8_t i = 0; i < sizeof(mSwitches); i++)
		mSwitches[i] = 0;

	if (modules == 0)
		return;

	uint8_t myByte = 0;
	uint8_t myBit = 0;

	digitalWrite(mLoadPin, HIGH);
	delayMicroseconds(TIME);
	digitalWrite(mClockPin, HIGH);
	delayMicroseconds(TIME);
	digitalWrite(mClockPin, LOW);
	delayMicroseconds(TIME);
	digitalWrite(mResetPin, HIGH);
	delayMicroseconds(TIME);
	digitalWrite(mResetPin, LOW);
	delayMicroseconds(TIME);
	digitalWrite(mLoadPin, LOW);

	delayMicroseconds(TIME / 2);
	bitWrite(mSwitches[myByte], myBit++, digitalRead(mDataPin));
	delayMicroseconds(TIME / 2);

	// uint16_t: up to 32 modules x 16 bits = 512 bits. With uint8_t the
	// counter overflowed at 255 and the loop never ended from 16 modules.
	const uint16_t bits = 16u * modules;

	for (uint16_t i = 1; i < bits; i++)
	{
		digitalWrite(mClockPin, HIGH);
		delayMicroseconds(TIME);
		digitalWrite(mClockPin, LOW);

		delayMicroseconds(TIME / 2);
		bitWrite(mSwitches[myByte], myBit++, digitalRead(mDataPin));

		if (myBit == 8)
		{
			myByte++;
			myBit = 0;
		}
		delayMicroseconds(TIME / 2);
	}
}

bool TrackReporterS88::getValue(uint16_t index) const
{
	if (index < 1 || index > 8u * sizeof(mSwitches))
		return false;
	index--;
	return bitRead(mSwitches[index / 8], index % 8);
}

uint8_t TrackReporterS88::getByte(uint8_t index) const
{
	if (index >= sizeof(mSwitches))
		return 0;
	return mSwitches[index];
}
