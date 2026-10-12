// Scripted CAN bus for the unit tests: records the frames sent by
// TrackController and returns the frames queued in 'rx'. 'onSend' lets a
// test answer like a Gleisbox or an MS2 would.
#pragma once

#include <CanDriver.h>
#include <deque>
#include <functional>
#include <vector>

class MockCanDriver final : public CanDriver
{
public:
    std::vector<CanFrame> sent;
    std::deque<CanFrame> rx;
    std::function<void(const CanFrame &)> onSend;
    bool failSend = false;
    bool failBegin = false;
    uint8_t rxPin = 0, txPin = 0;
    int beginCount = 0;

    bool begin(Print *) override
    {
        beginCount++;
        return !failBegin;
    }

    bool send(const CanFrame &f) override
    {
        if (failSend)
            return false;
        sent.push_back(f);
        if (onSend)
            onSend(f);
        return true;
    }

    bool receive(CanFrame &f) override
    {
        if (rx.empty())
            return false;
        f = rx.front();
        rx.pop_front();
        return true;
    }

    void setPins(uint8_t rx_, uint8_t tx_) override
    {
        rxPin = rx_;
        txPin = tx_;
    }

    // Number of frames sent with the given Märklin command.
    int countSent(uint8_t command) const
    {
        int n = 0;
        for (const CanFrame &f : sent)
            if (((f.id >> 17) & 0xFF) == command)
                n++;
        return n;
    }
};

// Builds an answer frame (response bit set) from a device with 'hash'.
inline CanFrame answer(uint8_t cmd, std::initializer_list<uint8_t> d, uint16_t hash = 0x3363)
{
    CanFrame f;
    f.id = (uint32_t(cmd) << 17) | (1UL << 16) | hash;
    f.ext = true;
    f.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d)
        f.data[i++] = b;
    return f;
}

inline uint8_t commandOf(const CanFrame &f)
{
    return (f.id >> 17) & 0xFF;
}
