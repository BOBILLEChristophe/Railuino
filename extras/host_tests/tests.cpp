// Host tests for the phase 1 fixes (ESP32 flavour, scripted CAN bus).
#include "TrackController.h"
#include "TrackReporterS88.h"
#include <ACAN_ESP32.h>
#include <cstdio>
#include <initializer_list>

uint32_t g_millis = 0;
uint32_t g_millisStep = 1;
int g_digitalReads = 0;
Print Serial;
ACAN_ESP32 ACAN_ESP32::can;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static CANMessage answer(uint8_t cmd, std::initializer_list<uint8_t> d, uint16_t hash = 0x1234)
{
    CANMessage m;
    m.id = (uint32_t(cmd) << 17) | (1UL << 16) | hash;
    m.ext = true;
    m.len = (uint8_t)d.size();
    uint8_t i = 0;
    for (uint8_t b : d) m.data[i++] = b;
    return m;
}

static void reset()
{
    ACAN_ESP32::can = ACAN_ESP32();
    g_millis = 0;
    g_millisStep = 1;
}

int main()
{
    puts("#1 constructors");
    {
        TrackController a(0xDF24);                 // main.cpp
        CHECK(a.getHash() == 0xDF24);
        CHECK(a.getTimeout() == TrackController::DEFAULT_TIMEOUT);
        TrackController b(0xDF24, true);           // TCPCommand.ino
        CHECK(b.getHash() == 0xDF24 && b.isDebug() && b.getTimeout() == 1000);
        const uint64_t TIMEOUT = 500;              // as in the examples
        TrackController c(0xDF24, false, TIMEOUT, true);
        CHECK(c.getHash() == 0xDF24 && c.getTimeout() == 500 && c.isLoopback());
        TrackController d;
        CHECK(d.getHash() == 0 && d.getTimeout() == 1000);
        d.setTimeout(250);
        CHECK(d.getTimeout() == 250);
    }

    puts("#3 response for another loco is ignored");
    {
        reset();
        TrackController ctrl(0xDF24);
        ACAN_ESP32::can.onSend = [](const CANMessage &m) {
            if (((m.id >> 17) & 0xFF) == 0x04) {
                // MS2 answer for loco 0x4008 first, then ours (0x4007).
                ACAN_ESP32::can.rx.push_back(answer(0x04, {0, 0, 0x40, 0x08, 0x01, 0x11}));
                ACAN_ESP32::can.rx.push_back(answer(0x04, {0, 0, 0x40, 0x07, 0x02, 0x22}));
            }
        };
        uint16_t speed = 0;
        CHECK(ctrl.getLocoSpeed(0x4007, &speed));
        CHECK(speed == 0x0222);
    }

    puts("#3 system sub-command must match");
    {
        reset();
        TrackController ctrl(0xDF24);
        ACAN_ESP32::can.onSend = [](const CANMessage &m) {
            if (((m.id >> 17) & 0xFF) == 0x00 && m.data[4] == 0x03) {
                ACAN_ESP32::can.rx.push_back(answer(0x00, {0, 0, 0x40, 0x07, 0x01})); // Go, not ours
            }
        };
        CHECK(!ctrl.emergency(0x4007));
        ACAN_ESP32::can.onSend = [](const CANMessage &m) {
            ACAN_ESP32::can.rx.push_back(answer(0x00, {0, 0, 0x40, 0x07, 0x03}));
            (void)m;
        };
        CHECK(ctrl.emergency(0x4007));
    }

    puts("#2 timeout across millis() wrap-around");
    {
        reset();
        TrackController ctrl(0xDF24, false, 1000);
        g_millis = 0xFFFFFF00u;                    // 256 ms before the wrap
        uint16_t speed;
        CHECK(!ctrl.getLocoSpeed(0x4007, &speed)); // must return
        uint32_t elapsed = g_millis - 0xFFFFFF00u;
        CHECK(elapsed >= 1000 && elapsed < 1100);
    }

    puts("#4 send error returns false, no freeze");
    {
        reset();
        TrackController ctrl(0xDF24, true);
        ACAN_ESP32::can.failSend = true;
        CHECK(!ctrl.setLocoSpeed(0x4007, 100));
    }

    puts("#7 getVersion ignores the MS2 and reads the Gleisbox");
    {
        reset();
        TrackController ctrl(0xDF24);
        ACAN_ESP32::can.onSend = [](const CANMessage &m) {
            if (((m.id >> 17) & 0xFF) == 0x18) {
                ACAN_ESP32::can.rx.push_back(answer(0x18, {0x4D, 0x54, 0, 0, 0x03, 0x14, 0x00, 0x32})); // MS2
                ACAN_ESP32::can.rx.push_back(answer(0x18, {0x47, 0x43, 0, 0, 0x01, 0x27, 0x00, 0x10})); // Gleisbox
            }
        };
        uint8_t hi = 0xEE, lo = 0xEE;
        CHECK(ctrl.getVersion(&hi, &lo));
        CHECK(hi == 0x01 && lo == 0x27);
    }

    puts("#8 begin() sends the wake-up message once");
    {
        reset();
        TrackController ctrl(0xDF24);
        ACAN_ESP32::can.onSend = [](const CANMessage &m) {
            if (((m.id >> 17) & 0xFF) == 0x1B)
                ACAN_ESP32::can.rx.push_back(answer(0x1B, {0, 0, 0, 0, 0x11}));
        };
        CHECK(ctrl.begin());
        int n = 0;
        for (auto &m : ACAN_ESP32::can.sent)
            if (((m.id >> 17) & 0xFF) == 0x1B) n++;
        CHECK(n == 1);

        reset();
        ACAN_ESP32::can.beginError = 0x10;
        TrackController ctrl2(0xDF24);
        CHECK(!ctrl2.begin());
        CHECK(ACAN_ESP32::can.sent.empty());
    }

    puts("#6 #15 TrackMessage print / parse round trip");
    {
        TrackMessage m;
        m.clear();
        m.hash = 0xDF24;
        m.response = true;
        m.command = 0x04;
        m.length = 6;
        uint8_t d[6] = {0, 0, 0x40, 0x07, 0x01, 0xF4};
        memcpy(m.data, d, 6);
        Print p;
        p.print(m);                                 // Printable
        CHECK(p.out == "df24 R 04 6 00 00 40 07 01 f4");
        String s(p.out);
        TrackMessage n;
        CHECK(n.parseFrom(s));
        CHECK(n.hash == 0xDF24 && n.response && n.command == 0x04 && n.length == 6);
        CHECK(memcmp(n.data, d, 6) == 0);
        String bad("dg24 R 04 0");
        CHECK(!n.parseFrom(bad));
    }

    puts("#12 accessory returns the real result");
    {
        reset();
        TrackController ctrl(0xDF24, false, 50);
        CHECK(!ctrl.setAccessory(0x3000, 1, 1, 0));   // nobody answers
        ACAN_ESP32::can.onSend = [](const CANMessage &m) {
            CANMessage a = m; a.id |= (1UL << 16);
            ACAN_ESP32::can.rx.push_back(a);           // Gleisbox echo
        };
        CHECK(ctrl.setAccessory(0x3000, 1, 1, 20));
        CHECK(ACAN_ESP32::can.sent.size() == 3);       // 1 failed try + on + off
        CHECK(ACAN_ESP32::can.sent.back().data[5] == 0); // coil switched off
    }

    puts("#13 accessory command line: power is read");
    {
        reset();
        TrackController ctrl(0xDF24, false, 10);
        ctrl.handleUserCommands("accessory 12288 1 1 20");
        CHECK(!ACAN_ESP32::can.sent.empty());
        const CANMessage &m = ACAN_ESP32::can.sent.front();
        CHECK(m.data[2] == 0x30 && m.data[3] == 0x00 && m.data[4] == 1 && m.data[5] == 1);
    }

    puts("#5 S88 with 32 modules terminates, indexes are checked");
    {
        TrackReporterS88 s88(40);                    // limited to 32
        g_digitalReads = 0;
        s88.refresh();
        CHECK(g_digitalReads == 512);
        CHECK(s88.getValue(1) == true);              // read #0 -> 1
        CHECK(s88.getValue(2) == false);
        CHECK(s88.getValue(0) == false && s88.getValue(513) == false);
        CHECK(s88.getByte(64) == 0);
        g_digitalReads = 0;
        s88.refresh(2);
        CHECK(g_digitalReads == 32);
    }

    printf("%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures != 0;
}
