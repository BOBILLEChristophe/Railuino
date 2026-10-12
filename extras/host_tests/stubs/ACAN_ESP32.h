// Mock of ACAN_ESP32 / ACAN2515: a scripted CAN bus for host tests.
#pragma once
#include "Arduino.h"
#include <deque>
#include <vector>
#include <functional>

typedef int gpio_num_t;

class CANMessage {
public:
    uint32_t id = 0;
    bool ext = false;
    bool rtr = false;
    uint8_t idx = 0;
    uint8_t len = 0;
    uint8_t data[8] = {0};
};

struct MockBus {
    std::vector<CANMessage> sent;
    std::deque<CANMessage> rx;
    // Called after each send; may push answers into rx.
    std::function<void(const CANMessage &)> onSend;
    bool failSend = false;
    uint32_t beginError = 0;

    bool tryToSend(const CANMessage &m) {
        if (failSend) return false;
        sent.push_back(m);
        if (onSend) onSend(m);
        return true;
    }
    bool receive(CANMessage &m) {
        if (rx.empty()) return false;
        m = rx.front();
        rx.pop_front();
        return true;
    }
};

struct SettingsBase {
    uint32_t mBitRatePrescaler = 4;
    bool mTripleSampling = false;
    uint32_t actualBitRate() const { return 250000; }
    bool exactBitRate() const { return true; }
    uint32_t samplePointFromBitStart() const { return 75; }
};

class ACAN_ESP32_Settings : public SettingsBase {
public:
    explicit ACAN_ESP32_Settings(uint32_t) {}
    gpio_num_t mRxPin = 0, mTxPin = 0;
};

class ACAN_ESP32 : public MockBus {
public:
    uint32_t begin(const ACAN_ESP32_Settings &) { return beginError; }
    static ACAN_ESP32 can;
};
