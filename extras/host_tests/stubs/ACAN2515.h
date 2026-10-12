#pragma once
#include "ACAN_ESP32.h"

struct SPIClass { void begin() {} };
extern SPIClass SPI;

class ACAN2515Settings : public SettingsBase {
public:
    ACAN2515Settings(uint32_t, uint32_t) {}
};

class ACAN2515 : public MockBus {
public:
    ACAN2515(uint8_t, SPIClass &, uint8_t) {}
    template <typename F> uint16_t begin(const ACAN2515Settings &, F) { return 0; }
    void isr() {}
};
