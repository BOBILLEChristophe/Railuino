
#include "TrackReporterS88.h"


const int DATA = A0;
const int CLOCK = 2;
const int LOAD = 3;
const int RESET = 4;

const int TIME = 50;

TrackReporterS88::TrackReporterS88(int modules) 
: mSize(modules)
{
	// pinMode(DATA, INPUT);
	pinMode(CLOCK, OUTPUT);
	pinMode(LOAD, OUTPUT);
	pinMode(RESET, OUTPUT);
}

void TrackReporterS88::refresh()
{
	refresh(mSize);
}

void TrackReporterS88::refresh(int inMaxSize) {
	int myByte = 0;
	int myBit = 0;

	for (uint8_t i = 0; i <= sizeof(mSwitches); i++) {
		mSwitches[i] = 0;
	}

	digitalWrite(LOAD, HIGH);
	delayMicroseconds(TIME);
	digitalWrite(CLOCK, HIGH);
	delayMicroseconds(TIME);
	digitalWrite(CLOCK, LOW);
	delayMicroseconds(TIME);
	digitalWrite(RESET, HIGH);
	delayMicroseconds(TIME);
	digitalWrite(RESET, LOW);
	delayMicroseconds(TIME);
	digitalWrite(LOAD, LOW);

	delayMicroseconds(TIME / 2);
	bitWrite(mSwitches[myByte], myBit++, digitalRead(DATA));
	delayMicroseconds(TIME / 2);

	for (uint8_t i = 1; i < 16 * mSize; i++) {
		digitalWrite(CLOCK, HIGH);
		delayMicroseconds(TIME);
		digitalWrite(CLOCK, LOW);

		delayMicroseconds(TIME / 2);
		bitWrite(mSwitches[myByte], myBit++, digitalRead(DATA));

		if (myBit == 8) {
			myByte++;
			myBit = 0;
		}
		delayMicroseconds(TIME / 2);
	}
}

bool TrackReporterS88::getValue(int index) {
	index--;
	return bitRead(mSwitches[index / 8], index % 8);
}


byte TrackReporterS88::getByte(int index) {
	return mSwitches[index];
}