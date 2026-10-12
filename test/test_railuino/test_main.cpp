// Unit tests of Railuino, run on the PC without any board:
//   pio test -e native
// The CAN bus is simulated by MockCanDriver; the clock by g_millis.
// Each test refers to the audit finding it covers (#1, #2...).

#include <unity.h>
#include <Railuino.h>
#include "MockCanDriver.h"

// Definitions required by the Arduino stubs (test/stubs/Arduino.h).
uint32_t g_millis = 0;
uint32_t g_millisStep = 1;
int g_digitalReads = 0;
Print Serial;

static MockCanDriver *bus;

void setUp()
{
    bus = new MockCanDriver();
    g_millis = 0;
    g_millisStep = 1;
}

void tearDown()
{
    delete bus;
}

// ---------------------------------------------------------------- #1

void test_constructors_take_the_hash_first()
{
    TrackController a(0xDF24);
    TEST_ASSERT_EQUAL_HEX16(0xDF24, a.getHash());
    TEST_ASSERT_EQUAL_UINT32(TrackController::DEFAULT_TIMEOUT, a.getTimeout());

    TrackController b(0xDF24, true);
    TEST_ASSERT_EQUAL_HEX16(0xDF24, b.getHash());
    TEST_ASSERT_TRUE(b.isDebug());

    const uint64_t TIMEOUT = 500; // as in the examples
    TrackController c(0xDF24, false, TIMEOUT, true);
    TEST_ASSERT_EQUAL_UINT32(500, c.getTimeout());
    TEST_ASSERT_TRUE(c.isLoopback());

    TrackController d;
    d.setTimeout(250);
    TEST_ASSERT_EQUAL_HEX16(0, d.getHash());
    TEST_ASSERT_EQUAL_UINT32(250, d.getTimeout());

    TrackController e(*bus, 0xDF24, true, 300);
    TEST_ASSERT_EQUAL_HEX16(0xDF24, e.getHash());
    TEST_ASSERT_EQUAL_UINT32(300, e.getTimeout());
}

// ---------------------------------------------------------------- drivers

void test_without_driver_on_native_begin_fails()
{
    TrackController ctrl(0xDF24); // no default driver on the PC
    TEST_ASSERT_FALSE(ctrl.begin());
    TEST_ASSERT_FALSE(ctrl.setPower(true));
}

void test_begin_passes_pins_to_the_driver()
{
    TrackController ctrl(*bus, 0xDF24);
    TEST_ASSERT_TRUE(ctrl.begin(22, 23));
    TEST_ASSERT_EQUAL_UINT8(22, bus->rxPin);
    TEST_ASSERT_EQUAL_UINT8(23, bus->txPin);
    TEST_ASSERT_EQUAL_INT(1, bus->beginCount);
}

void test_begin_reports_a_driver_error()
{
    bus->failBegin = true;
    TrackController ctrl(*bus, 0xDF24);
    TEST_ASSERT_FALSE(ctrl.begin());
    TEST_ASSERT_TRUE(bus->sent.empty());
}

void test_frames_are_encoded_as_marklin_ids()
{
    TrackController ctrl(*bus, 0xDF24, false, 10);
    ctrl.setLocoSpeed(0x4005, 0x1C);
    TEST_ASSERT_EQUAL_size_t(1, bus->sent.size());
    const CanFrame &f = bus->sent[0];
    TEST_ASSERT_EQUAL_HEX32(0x8DF24, f.id); // seen on the layout: speed, hash DF24
    TEST_ASSERT_TRUE(f.ext);
    TEST_ASSERT_EQUAL_UINT8(6, f.len);
    const uint8_t expected[6] = {0x00, 0x00, 0x40, 0x05, 0x00, 0x1C};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, f.data, 6);
}

void test_standard_frames_are_ignored()
{
    TrackController ctrl(*bus, 0xDF24);
    CanFrame standard = answer(0x04, {0, 0, 0x40, 0x07, 0, 1});
    standard.ext = false;
    bus->rx.push_back(standard);
    TrackMessage m;
    TEST_ASSERT_FALSE(ctrl.receiveMessage(m));
}

// ---------------------------------------------------------------- #3

void test_answer_for_another_loco_is_ignored()
{
    TrackController ctrl(*bus, 0xDF24);
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x04)
        {
            bus->rx.push_back(answer(0x04, {0, 0, 0x40, 0x08, 0x01, 0x11}));
            bus->rx.push_back(answer(0x04, {0, 0, 0x40, 0x07, 0x02, 0x22}));
        }
    };
    uint16_t speed = 0;
    TEST_ASSERT_TRUE(ctrl.getLocoSpeed(0x4007, &speed));
    TEST_ASSERT_EQUAL_HEX16(0x0222, speed);
}

void test_system_sub_command_must_match()
{
    TrackController ctrl(*bus, 0xDF24);
    bus->onSend = [](const CanFrame &) {
        bus->rx.push_back(answer(0x00, {0, 0, 0x40, 0x07, 0x01})); // Go, not ours
    };
    TEST_ASSERT_FALSE(ctrl.emergency(0x4007));

    bus->onSend = [](const CanFrame &) {
        bus->rx.push_back(answer(0x00, {0, 0, 0x40, 0x07, 0x03}));
    };
    TEST_ASSERT_TRUE(ctrl.emergency(0x4007));
}

void test_ms2_ping_between_request_and_answer()
{
    // Sequence seen on the layout: an MS2 ping and the Gleisbox answer
    // arrive between our function command and its answer.
    TrackController ctrl(*bus, 0xDF24);
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) != 0x06)
            return;
        CanFrame ping;
        ping.id = (0x18UL << 17) | 0x8B23;
        bus->rx.push_back(ping);
        bus->rx.push_back(answer(0x18, {0x47, 0x46, 0xA4, 0x75, 0x01, 0x2F, 0x00, 0x11}));
        bus->rx.push_back(answer(0x06, {0, 0, 0x40, 0x05, 0x02, 0x01}));
    };
    TEST_ASSERT_TRUE(ctrl.setLocoFunction(0x4005, 2, 1));
}

// ---------------------------------------------------------------- #2 #4

void test_timeout_across_millis_wrap_around()
{
    TrackController ctrl(*bus, 0xDF24, false, 1000);
    g_millis = 0xFFFFFF00u; // 256 ms before the wrap
    uint16_t speed;
    TEST_ASSERT_FALSE(ctrl.getLocoSpeed(0x4007, &speed));
    const uint32_t elapsed = g_millis - 0xFFFFFF00u;
    TEST_ASSERT_TRUE(elapsed >= 1000 && elapsed < 1100);
}

void test_send_error_returns_false()
{
    TrackController ctrl(*bus, 0xDF24, true);
    bus->failSend = true;
    TEST_ASSERT_FALSE(ctrl.setLocoSpeed(0x4007, 100));
}

// ---------------------------------------------------------------- #7 #8

void test_get_version_reads_the_gleisbox()
{
    TrackController ctrl(*bus, 0xDF24);
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x18)
        {
            bus->rx.push_back(answer(0x18, {0x4D, 0x54, 0, 0, 0x03, 0x14, 0x00, 0x32}, 0x8B23)); // MS2
            bus->rx.push_back(answer(0x18, {0x47, 0x46, 0xA4, 0x75, 0x01, 0x2F, 0x00, 0x11}));   // Gleisbox
        }
    };
    uint8_t hi = 0, lo = 0;
    TEST_ASSERT_TRUE(ctrl.getVersion(&hi, &lo));
    TEST_ASSERT_EQUAL_HEX8(0x01, hi);
    TEST_ASSERT_EQUAL_HEX8(0x2F, lo);
}

void test_begin_sends_the_wake_up_once_without_waiting()
{
    TrackController ctrl(*bus, 0xDF24);
    TEST_ASSERT_TRUE(ctrl.begin());
    TEST_ASSERT_EQUAL_INT(1, bus->countSent(0x1B));
    TEST_ASSERT_TRUE(g_millis < 600); // only the 500 ms settle delay
}

// ---------------------------------------------------------------- #6 #15

void test_track_message_print_parse_round_trip()
{
    TrackMessage m;
    m.clear();
    m.hash = 0xDF24;
    m.response = true;
    m.command = 0x04;
    m.length = 6;
    const uint8_t d[6] = {0, 0, 0x40, 0x07, 0x01, 0xF4};
    memcpy(m.data, d, 6);

    Print p;
    p.print(m); // Printable
    TEST_ASSERT_EQUAL_STRING("df24 R 04 6 00 00 40 07 01 f4", p.out.c_str());

    String s(p.out);
    TrackMessage n;
    TEST_ASSERT_TRUE(n.parseFrom(s));
    TEST_ASSERT_EQUAL_HEX16(0xDF24, n.hash);
    TEST_ASSERT_TRUE(n.response);
    TEST_ASSERT_EQUAL_HEX8(0x04, n.command);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(d, n.data, 6);

    String bad("dg24 R 04 0");
    TEST_ASSERT_FALSE(n.parseFrom(bad));
}

// ---------------------------------------------------------------- #12 #13

void test_accessory_returns_the_real_result()
{
    TrackController ctrl(*bus, 0xDF24, false, 50);
    TEST_ASSERT_FALSE(ctrl.setAccessory(0x3000, 1, 1, 0)); // nobody answers

    bus->onSend = [](const CanFrame &f) {
        CanFrame a = f;
        a.id |= (1UL << 16);
        bus->rx.push_back(a); // Gleisbox echo
    };
    TEST_ASSERT_TRUE(ctrl.setAccessory(0x3000, 1, 1, 20));
    TEST_ASSERT_EQUAL_size_t(3, bus->sent.size()); // failed try + on + off
    TEST_ASSERT_EQUAL_HEX8(0, bus->sent.back().data[5]);
}

void test_accessory_command_line_reads_the_power()
{
    TrackController ctrl(*bus, 0xDF24, false, 10);
    ctrl.handleUserCommands("accessory 12288 1 1 20");
    TEST_ASSERT_FALSE(bus->sent.empty());
    const CanFrame &f = bus->sent.front();
    const uint8_t expected[4] = {0x30, 0x00, 1, 1};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, &f.data[2], 4);
}

// ---------------------------------------------------------------- #5

void test_s88_with_32_modules()
{
    TrackReporterS88 s88(40); // limited to 32
    g_digitalReads = 0;
    s88.refresh();
    TEST_ASSERT_EQUAL_INT(512, g_digitalReads);
    TEST_ASSERT_TRUE(s88.getValue(1)); // read #0 -> 1
    TEST_ASSERT_FALSE(s88.getValue(2));
    TEST_ASSERT_FALSE(s88.getValue(0));
    TEST_ASSERT_FALSE(s88.getValue(513));
    TEST_ASSERT_EQUAL_UINT8(0, s88.getByte(64));
    g_digitalReads = 0;
    s88.refresh(2);
    TEST_ASSERT_EQUAL_INT(32, g_digitalReads);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_constructors_take_the_hash_first);
    RUN_TEST(test_without_driver_on_native_begin_fails);
    RUN_TEST(test_begin_passes_pins_to_the_driver);
    RUN_TEST(test_begin_reports_a_driver_error);
    RUN_TEST(test_frames_are_encoded_as_marklin_ids);
    RUN_TEST(test_standard_frames_are_ignored);
    RUN_TEST(test_answer_for_another_loco_is_ignored);
    RUN_TEST(test_system_sub_command_must_match);
    RUN_TEST(test_ms2_ping_between_request_and_answer);
    RUN_TEST(test_timeout_across_millis_wrap_around);
    RUN_TEST(test_send_error_returns_false);
    RUN_TEST(test_get_version_reads_the_gleisbox);
    RUN_TEST(test_begin_sends_the_wake_up_once_without_waiting);
    RUN_TEST(test_track_message_print_parse_round_trip);
    RUN_TEST(test_accessory_returns_the_real_result);
    RUN_TEST(test_accessory_command_line_reads_the_power);
    RUN_TEST(test_s88_with_32_modules);
    return UNITY_END();
}
