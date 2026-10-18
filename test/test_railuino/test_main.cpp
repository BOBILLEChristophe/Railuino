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

void test_answer_must_match_function_cv_and_length()
{
    TrackController ctrl(*bus, 0xDF24, false, 50);

    // The MS2 switches F1 of the same loco while we read F0: its answer
    // comes first and must not be taken as ours.
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x06)
        {
            bus->rx.push_back(answer(0x06, {0, 0, 0x40, 0x07, 1, 1}));
            bus->rx.push_back(answer(0x06, {0, 0, 0x40, 0x07, 0, 0}));
        }
    };
    uint8_t power = 9;
    TEST_ASSERT_TRUE(ctrl.getLocoFunction(0x4007, 0, &power));
    TEST_ASSERT_EQUAL_UINT8(0, power);

    // Speed answer of 5 bytes: incomplete, refused.
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x04)
            bus->rx.push_back(answer(0x04, {0, 0, 0x40, 0x07, 0x02}));
    };
    uint16_t speed = 0;
    TEST_ASSERT_FALSE(ctrl.getLocoSpeed(0x4007, &speed));

    // CV read: the answer for CV 2 is not the one for CV 1.
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x07)
        {
            bus->rx.push_back(answer(0x07, {0, 0, 0x40, 0x07, 0, 2, 55}));
            bus->rx.push_back(answer(0x07, {0, 0, 0x40, 0x07, 0, 1, 7}));
        }
    };
    uint8_t value = 0;
    TEST_ASSERT_TRUE(ctrl.readConfig(0x4007, 1, &value));
    TEST_ASSERT_EQUAL_UINT8(7, value);
}

void test_read_config_refused_at_once()
{
    TrackController ctrl(*bus, 0xDF24);
    // The Gleisbox could not read the CV: answer without a value (DLC 6).
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x07)
            bus->rx.push_back(answer(0x07, {0, 0, 0x40, 0x07, 0, 1}));
    };
    uint8_t value = 99;
    const uint32_t start = g_millis;
    TEST_ASSERT_FALSE(ctrl.readConfig(0x4007, 1, &value));
    TEST_ASSERT_EQUAL_UINT8(99, value);
    TEST_ASSERT_TRUE(g_millis - start < 1000); // not the 10 s timeout
}

void test_config_cv_number_and_index()
{
    TrackController ctrl(*bus, 0xDF24);
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x07)
            bus->rx.push_back(answer(0x07, {0, 0, 0x40, 0x07, f.data[4], f.data[5], 42}));
    };
    uint8_t value = 0;
    // CV 0x123, index 5: data[4] = 5 << 2 | 0x01, data[5] = 0x23.
    TEST_ASSERT_TRUE(ctrl.readConfig(0x4007, 0x123, &value, 5));
    TEST_ASSERT_EQUAL_UINT8(42, value);
    const CanFrame &sent = bus->sent.back();
    TEST_ASSERT_EQUAL_UINT8(7, sent.len);
    TEST_ASSERT_EQUAL_HEX8(0x15, sent.data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x23, sent.data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x01, sent.data[6]);
}

void test_write_config_main_track_and_result()
{
    TrackController ctrl(*bus, 0xDF24);
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x08)
            bus->rx.push_back(answer(0x08, {0, 0, 0xC0, 0x03, 0, 3, 12, 0xC0}));
    };
    uint8_t result = 0;
    TEST_ASSERT_TRUE(ctrl.writeConfig(0xC003, 3, 12, 0, true, &result));
    const CanFrame &sent = bus->sent.back();
    TEST_ASSERT_EQUAL_UINT8(8, sent.len);
    TEST_ASSERT_EQUAL_HEX8(0x00, sent.data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x03, sent.data[5]);
    TEST_ASSERT_EQUAL_HEX8(12, sent.data[6]);
    TEST_ASSERT_EQUAL_HEX8(0x80, sent.data[7]); // main track (POM)
    TEST_ASSERT_EQUAL_HEX8(0xC0, result);       // written and verified

    // Default call: main track too; programming track on request.
    TEST_ASSERT_TRUE(ctrl.writeConfig(0xC003, 3, 12));
    TEST_ASSERT_EQUAL_HEX8(0x80, bus->sent.back().data[7]);
    TEST_ASSERT_TRUE(ctrl.writeConfig(0xC003, 3, 12, 0, false));
    TEST_ASSERT_EQUAL_HEX8(0x00, bus->sent.back().data[7]);
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

static void echoAccessories()
{
    bus->onSend = [](const CanFrame &f) {
        CanFrame a = f;
        a.id |= (1UL << 16);
        bus->rx.push_back(a); // Gleisbox echo
    };
}

void test_async_accessory_switches_off_in_poll()
{
    TrackController ctrl(*bus, 0xDF24, false, 50);
    echoAccessories();
    ctrl.setAccessoryAsync(true);

    const uint32_t start = g_millis;
    TEST_ASSERT_TRUE(ctrl.setTurnout(0x3000, true));
    TEST_ASSERT_EQUAL_size_t(1, bus->sent.size());   // on only
    TEST_ASSERT_EQUAL_HEX8(1, bus->sent[0].data[5]);
    TEST_ASSERT_TRUE(g_millis - start < 100);          // did not wait
    TEST_ASSERT_EQUAL_UINT8(1, ctrl.pendingAccessories());

    ctrl.poll(); // too early
    TEST_ASSERT_EQUAL_size_t(1, bus->sent.size());

    g_millis = start + 1100; // 1000 ms pulse, plus the time of the exchange
    ctrl.poll();
    TEST_ASSERT_EQUAL_size_t(2, bus->sent.size());
    TEST_ASSERT_EQUAL_HEX8(ACC_STRAIGHT, bus->sent[1].data[4]);
    TEST_ASSERT_EQUAL_HEX8(0, bus->sent[1].data[5]);   // off
    TEST_ASSERT_EQUAL_UINT8(0, ctrl.pendingAccessories());
}

void test_async_accessory_switched_again_turns_previous_coil_off()
{
    TrackController ctrl(*bus, 0xDF24, false, 50);
    echoAccessories();
    ctrl.setAccessoryAsync(true);
    g_millis = 0xFFFFFF00UL; // pulse across the millis() wrap-around

    ctrl.setTurnout(0x3000, true);
    ctrl.setTurnout(0x3001, true);
    ctrl.setTurnout(0x3000, false); // straight coil off first, then round on
    TEST_ASSERT_EQUAL_size_t(4, bus->sent.size());
    TEST_ASSERT_EQUAL_HEX8(ACC_STRAIGHT, bus->sent[2].data[4]);
    TEST_ASSERT_EQUAL_HEX8(0, bus->sent[2].data[5]);
    TEST_ASSERT_EQUAL_HEX8(ACC_ROUND, bus->sent[3].data[4]);
    TEST_ASSERT_EQUAL_HEX8(1, bus->sent[3].data[5]);
    TEST_ASSERT_EQUAL_UINT8(2, ctrl.pendingAccessories());

    g_millis += 2000; // wraps
    ctrl.poll();
    TEST_ASSERT_EQUAL_UINT8(0, ctrl.pendingAccessories());
    TEST_ASSERT_EQUAL_size_t(6, bus->sent.size());

    // Back to blocking: waits, and end() leaves no coil on.
    ctrl.setTurnout(0x3002, true);
    TEST_ASSERT_EQUAL_UINT8(1, ctrl.pendingAccessories());
    ctrl.end();
    TEST_ASSERT_EQUAL_UINT8(0, ctrl.pendingAccessories());
    ctrl.setAccessoryAsync(false);
    const size_t before = bus->sent.size();
    ctrl.setTurnout(0x3002, false);
    TEST_ASSERT_EQUAL_size_t(before + 2, bus->sent.size()); // on + off
}

static int offAnswersToSkip = 0;

static size_t offFrames()
{
    size_t n = 0;
    for (const CanFrame &f : bus->sent)
        if (commandOf(f) == 0x0B && f.data[5] == 0)
            n++;
    return n;
}

void test_switch_off_is_retried_then_given_up()
{
    TrackController ctrl(*bus, 0xDF24, false, 20);
    // On is acknowledged; the first switch-off is not.
    offAnswersToSkip = 1;
    bus->onSend = [](const CanFrame &f) {
        if (f.data[5] == 0 && offAnswersToSkip > 0)
        {
            offAnswersToSkip--;
            return;
        }
        CanFrame a = f;
        a.id |= (1UL << 16);
        bus->rx.push_back(a);
    };
    ctrl.setAccessoryAsync(true);
    ctrl.setTurnoutPulse(200);
    TEST_ASSERT_EQUAL_UINT16(200, ctrl.getTurnoutPulse());

    const uint32_t start = g_millis;
    ctrl.setTurnout(0x3000, true);
    g_millis = start + 300;
    ctrl.poll(); // not acknowledged: kept for a retry
    TEST_ASSERT_EQUAL_size_t(1, offFrames());
    TEST_ASSERT_EQUAL_UINT8(1, ctrl.pendingAccessories());
    g_millis += 100;
    ctrl.poll(); // acknowledged
    TEST_ASSERT_EQUAL_size_t(2, offFrames());
    TEST_ASSERT_EQUAL_UINT8(0, ctrl.pendingAccessories());

    // Never acknowledged: 3 attempts, then given up.
    offAnswersToSkip = 100;
    ctrl.setTurnout(0x3001, true);
    for (int i = 0; i < 5; i++)
    {
        g_millis += 300;
        ctrl.poll();
    }
    TEST_ASSERT_EQUAL_size_t(5, offFrames());
    TEST_ASSERT_EQUAL_UINT8(0, ctrl.pendingAccessories());

    // Blocking mode retries too.
    ctrl.setAccessoryAsync(false);
    offAnswersToSkip = 2;
    TEST_ASSERT_TRUE(ctrl.setTurnout(0x3002, false));
    TEST_ASSERT_EQUAL_size_t(8, offFrames());
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


// ---------------------------------------------------------------- phase 4

void test_message_builder_and_uid()
{
    TrackMessage m = TrackMessage::make(Marklin::Cmd::Speed, 0x4007, 6);
    TEST_ASSERT_EQUAL_HEX8(0x04, m.command);
    TEST_ASSERT_EQUAL_UINT8(6, m.length);
    const uint8_t uid[4] = {0x00, 0x00, 0x40, 0x07};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(uid, m.data, 4);
    TEST_ASSERT_EQUAL_HEX32(0x4007, m.uid());

    m.setUid(0xFFE9B194UL); // MFX UID seen on the layout
    TEST_ASSERT_EQUAL_HEX32(0xFFE9B194UL, m.uid());

    TrackMessage s = TrackMessage::makeSystem(Marklin::SysCmd::Go);
    TEST_ASSERT_EQUAL_HEX8(0x00, s.command);
    TEST_ASSERT_EQUAL_UINT8(5, s.length);
    TEST_ASSERT_EQUAL_HEX8(0x01, s.data[4]);

    TrackMessage c;
    c.clear();
    c.setCanId(0x93363); // speed answer from the Gleisbox
    TEST_ASSERT_EQUAL_HEX8(0x04, c.command);
    TEST_ASSERT_TRUE(c.response);
    TEST_ASSERT_EQUAL_HEX16(0x3363, c.hash);
    TEST_ASSERT_EQUAL_HEX32(0x93363, c.canId());
}

static int handled = 0;
static TrackMessage lastHandled;
static void handler(const TrackMessage &m, void *context)
{
    handled++;
    lastHandled = m;
    if (context != nullptr)
        (*static_cast<int *>(context))++;
}

void test_poll_passes_ms2_commands_to_the_handler()
{
    TrackController ctrl(*bus, 0xDF24);
    int calls = 0;
    handled = 0;
    ctrl.onMessage(handler, &calls);
    // The MS2 (hash 8B23) drives loco 0x4007 at speed 100.
    CanFrame f;
    f.id = (0x04UL << 17) | 0x8B23;
    f.len = 6;
    const uint8_t d[6] = {0, 0, 0x40, 0x07, 0x00, 0x64};
    memcpy(f.data, d, 6);
    bus->rx.push_back(f);
    ctrl.poll();
    TEST_ASSERT_EQUAL_INT(1, handled);
    TEST_ASSERT_EQUAL_INT(1, calls);
    TEST_ASSERT_EQUAL_HEX32(0x4007, lastHandled.uid());
    TEST_ASSERT_EQUAL_HEX16(0x8B23, lastHandled.hash);
}

void test_messages_received_while_waiting_are_not_lost()
{
    TrackController ctrl(*bus, 0xDF24);
    handled = 0;
    ctrl.onMessage(handler);
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) != 0x06)
            return;
        CanFrame ping;
        ping.id = (0x18UL << 17) | 0x8B23;
        bus->rx.push_back(ping);
        bus->rx.push_back(answer(0x06, {0, 0, 0x40, 0x05, 0x02, 0x01}));
    };
    TEST_ASSERT_TRUE(ctrl.setLocoFunction(0x4005, 2, 1));
    TEST_ASSERT_EQUAL_INT(1, handled); // the MS2 ping, not our answer
    TEST_ASSERT_EQUAL_HEX8(0x18, lastHandled.command);
}

void test_log_goes_to_the_chosen_output()
{
    Serial.out.clear();
    TrackController quiet(*bus, 0xDF24, false, 10);
    quiet.setLocoSpeed(0x4005, 1);
    TEST_ASSERT_TRUE(Serial.out.empty());
    TEST_ASSERT_FALSE(quiet.isDebug());

    Print other;
    quiet.setLog(&other);
    TEST_ASSERT_TRUE(quiet.isDebug());
    quiet.setLocoSpeed(0x4005, 1);
    TEST_ASSERT_TRUE(Serial.out.empty());
    TEST_ASSERT_TRUE(other.out.find("<== ID : 0x8DF24") != std::string::npos);
}

void test_direction_enum()
{
    TrackController ctrl(*bus, 0xDF24, false, 10);
    ctrl.setLocoDirection(0x4005, Marklin::Direction::Reverse);
    TEST_ASSERT_EQUAL_HEX8(0x05, commandOf(bus->sent.back()));
    TEST_ASSERT_EQUAL_HEX8(2, bus->sent.back().data[4]);
    ctrl.toggleLocoDirection(0x4005);
    TEST_ASSERT_EQUAL_HEX8(3, bus->sent.back().data[4]);
}

void test_command_parser()
{
    TrackController ctrl(*bus, 0xDF24, false, 10);
    bus->onSend = [](const CanFrame &f) {
        CanFrame a = f;
        a.id |= (1UL << 16);
        bus->rx.push_back(a); // Gleisbox echo
    };
    CommandParser parser(ctrl);
    Print out;

    TEST_ASSERT_TRUE(parser.execute("speed 0x4007 300", &out));
    const CanFrame &f = bus->sent.back();
    TEST_ASSERT_EQUAL_HEX8(0x04, commandOf(f));
    const uint8_t speed[6] = {0, 0, 0x40, 0x07, 0x01, 0x2C};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(speed, f.data, 6);
    TEST_ASSERT_EQUAL_STRING("ok\n", out.out.c_str());

    // Old formats still work, with any spacing and case.
    TEST_ASSERT_TRUE(parser.execute("speed 16391 100"));
    TEST_ASSERT_TRUE(parser.execute("  FUNCTION   16391 10 1\r"));
    TEST_ASSERT_EQUAL_HEX8(10, bus->sent.back().data[4]); // function 10 (was limited to 0..9)
    TEST_ASSERT_TRUE(parser.execute("accessory 12289 1 1"));
    TEST_ASSERT_TRUE(parser.execute("direction 16391 3"));
    TEST_ASSERT_TRUE(parser.execute("power 1"));

    out.out.clear();
    TEST_ASSERT_FALSE(parser.execute("speed 16391", &out));
    TEST_ASSERT_EQUAL_STRING("error: usage: speed ADDR SPEED(0..1000)\n", out.out.c_str());
    // Decimal or 0x only: no octal, no sign, no overflow, speed <= 1000.
    TEST_ASSERT_TRUE(parser.execute("speed 16391 0300"));
    TEST_ASSERT_EQUAL_HEX8(0x01, bus->sent.back().data[4]); // 300, not 192
    TEST_ASSERT_EQUAL_HEX8(0x2C, bus->sent.back().data[5]);
    TEST_ASSERT_FALSE(parser.execute("speed -1 100"));
    TEST_ASSERT_FALSE(parser.execute("speed 16391 +5"));
    TEST_ASSERT_FALSE(parser.execute("speed 4294967296 100"));
    TEST_ASSERT_FALSE(parser.execute("speed 0x 100"));
    TEST_ASSERT_FALSE(parser.execute("speed 0x4007 1001"));
    TEST_ASSERT_TRUE(parser.execute("speed 0xFFFFFFFF 1000"));

    out.out.clear();
    TEST_ASSERT_FALSE(parser.execute("fly 1", &out));
    TEST_ASSERT_EQUAL_STRING("error: unknown command (type help)\n", out.out.c_str());
    TEST_ASSERT_FALSE(parser.execute("speed abc 1"));
    TEST_ASSERT_FALSE(parser.execute("power 2"));
    TEST_ASSERT_FALSE(parser.execute(""));
    std::string longLine(100, 'x');
    TEST_ASSERT_FALSE(parser.execute(longLine.c_str()));
}

void test_command_parser_reports_no_answer()
{
    TrackController ctrl(*bus, 0xDF24, false, 10); // nobody answers
    CommandParser parser(ctrl);
    Print out;
    TEST_ASSERT_FALSE(parser.execute("speed 16391 100", &out));
    TEST_ASSERT_EQUAL_STRING("error: no answer from the layout\n", out.out.c_str());
}

// ---------------------------------------------------------------- phase 5: MFX

static CanFrame mfxFrame(uint8_t cmd, bool response, uint16_t hash, uint32_t uid, uint16_t sid, int ask = -1)
{
    CanFrame f;
    f.id = (uint32_t(cmd) << 17) | (response ? (1UL << 16) : 0) | hash;
    f.len = ask < 0 ? 6 : 7;
    f.data[0] = uid >> 24;
    f.data[1] = uid >> 16;
    f.data[2] = uid >> 8;
    f.data[3] = uid;
    f.data[4] = sid >> 8;
    f.data[5] = sid;
    if (ask >= 0)
        f.data[6] = ask;
    return f;
}

static CanFrame discoveryAnswer(uint32_t uid)
{
    CanFrame f;
    f.id = (0x01UL << 17) | (1UL << 16) | 0x3363;
    f.len = 5;
    f.data[0] = uid >> 24;
    f.data[1] = uid >> 16;
    f.data[2] = uid >> 8;
    f.data[3] = uid;
    f.data[4] = 0x20;
    return f;
}

static int changes = 0;
static uint16_t lastLocId = 0;
static void onMfxChange(uint32_t, uint16_t locId, void *)
{
    changes++;
    lastLocId = locId;
}

void test_mfx_observer_learns_from_the_ms2()
{
    TrackController ctrl(*bus, 0xDF24);
    MfxRegistry mfx(ctrl);
    mfx.begin();
    changes = 0;
    mfx.onChange(onMfxChange);

    // The MS2 (8B23) binds UID FFE9B194 to SID 7, the Gleisbox answers.
    bus->rx.push_back(mfxFrame(0x02, false, 0x8B23, 0xFFE9B194UL, 7));
    bus->rx.push_back(mfxFrame(0x02, true, 0x3363, 0xFFE9B194UL, 7));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX16(0x4007, mfx.locIdOf(0xFFE9B194UL));
    TEST_ASSERT_EQUAL_HEX32(0xFFE9B194UL, mfx.uidOf(0x4007));
    TEST_ASSERT_EQUAL_HEX32(0xFFE9B194UL, mfx.uidOf(7));
    TEST_ASSERT_EQUAL_INT(1, changes);
    TEST_ASSERT_EQUAL_HEX16(0x4007, lastLocId);

    // Later the MS2 gives it SID 9: the table follows.
    bus->rx.push_back(mfxFrame(0x02, false, 0x8B23, 0xFFE9B194UL, 9));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX16(0x4009, mfx.locIdOf(0xFFE9B194UL));
    TEST_ASSERT_EQUAL_INT(2, changes);

    // Another loco takes SID 9: the first one is no longer valid.
    bus->rx.push_back(mfxFrame(0x02, false, 0x8B23, 0x11223344UL, 9));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX16(0x4009, mfx.locIdOf(0x11223344UL));
    TEST_ASSERT_EQUAL_HEX16(0, mfx.locIdOf(0xFFE9B194UL));
    TEST_ASSERT_EQUAL_UINT8(2, mfx.size());

    // An observer never sends anything.
    TEST_ASSERT_TRUE(bus->sent.empty());
}

void test_mfx_list_is_sorted_by_sid()
{
    TrackController ctrl(*bus, 0xDF24);
    MfxRegistry mfx(ctrl);
    mfx.begin();
    TEST_ASSERT_TRUE(mfx.assign(0xAAAA0001UL, 13));
    TEST_ASSERT_TRUE(mfx.assign(0xAAAA0002UL, 5));
    TEST_ASSERT_TRUE(mfx.assign(0xAAAA0003UL, 9));
    TEST_ASSERT_TRUE(mfx.assign(0xAAAA0004UL, 6));

    Print out;
    mfx.printTo(out);
    const size_t p5 = out.out.find("SID 5 ");
    const size_t p6 = out.out.find("SID 6 ");
    const size_t p9 = out.out.find("SID 9 ");
    const size_t p13 = out.out.find("SID 13 ");
    TEST_ASSERT_TRUE(p5 != std::string::npos && p6 != std::string::npos);
    TEST_ASSERT_TRUE(p9 != std::string::npos && p13 != std::string::npos);
    TEST_ASSERT_TRUE(p5 < p6 && p6 < p9 && p9 < p13);
    TEST_ASSERT_EQUAL_UINT8(4, mfx.size()); // the table itself is unchanged
    TEST_ASSERT_EQUAL_HEX32(0xAAAA0001UL, mfx.entry(0).uid);
}

void test_mfx_assign_seeds_the_table()
{
    TrackController ctrl(*bus, 0xDF24);
    MfxRegistry mfx(ctrl);
    mfx.begin();

    // From a Loc-ID or a SID; 0 is refused.
    TEST_ASSERT_TRUE(mfx.assign(0xFFE9B194UL, 0x4007));
    TEST_ASSERT_EQUAL_HEX16(0x4007, mfx.locIdOf(0xFFE9B194UL));
    TEST_ASSERT_TRUE(mfx.assign(0x11223344UL, 8));
    TEST_ASSERT_EQUAL_HEX16(0x4008, mfx.locIdOf(0x11223344UL));
    TEST_ASSERT_FALSE(mfx.assign(0, 5));
    TEST_ASSERT_FALSE(mfx.assign(0x55667788UL, 0));
    TEST_ASSERT_FALSE(mfx.assign(0x55667788UL, 0x4000));
    TEST_ASSERT_FALSE(mfx.assign(0x55667788UL, 0x8001)); // SX2 address
    TEST_ASSERT_TRUE(mfx.assign(0x55667788UL, 0x7FFF));  // last MFX Loc-ID
    TEST_ASSERT_EQUAL_HEX16(0x7FFF, mfx.locIdOf(0x55667788UL));
    TEST_ASSERT_FALSE(mfx.setFirstSid(0));
    TEST_ASSERT_FALSE(mfx.setFirstSid(0x4000));
    TEST_ASSERT_TRUE(mfx.setFirstSid(10));
    TEST_ASSERT_EQUAL_UINT8(3, mfx.size());

    // A later Bind of the MS2 wins.
    bus->rx.push_back(mfxFrame(0x02, false, 0x8B23, 0xFFE9B194UL, 9));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX16(0x4009, mfx.locIdOf(0xFFE9B194UL));
    TEST_ASSERT_TRUE(bus->sent.empty());
}

void test_mfx_observer_ignores_discovery_and_verify_sid_0()
{
    TrackController ctrl(*bus, 0xDF24);
    MfxRegistry mfx(ctrl);
    mfx.begin();
    bus->rx.push_back(discoveryAnswer(0xFFE9B194UL));
    bus->rx.push_back(mfxFrame(0x03, true, 0x3363, 0xFFE9B194UL, 0, 1));
    ctrl.poll();
    TEST_ASSERT_EQUAL_UINT8(0, mfx.size());
    TEST_ASSERT_TRUE(bus->sent.empty());
    TEST_ASSERT_FALSE(mfx.startDiscovery());
}

void test_mfx_master_discovery_bind_verify()
{
    TrackController ctrl(*bus, 0xDF24);
    MfxRegistry mfx(ctrl, MfxRegistry::Mode::Master);
    mfx.begin(); // start sequence: Stop, wake-up, counter, Verify, Go, Discovery
    TEST_ASSERT_EQUAL_size_t(6, bus->sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x00, commandOf(bus->sent[0]));
    TEST_ASSERT_EQUAL_HEX8(0x1B, commandOf(bus->sent[1]));
    TEST_ASSERT_EQUAL_HEX8(0x09, bus->sent[2].data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x03, commandOf(bus->sent[3]));
    TEST_ASSERT_EQUAL_HEX8(0x01, bus->sent[4].data[4]);
    TEST_ASSERT_EQUAL_HEX8(0x01, commandOf(bus->sent[5]));
    bus->sent.clear();

    // The Gleisbox finds the loco: Bind with SID 5.
    bus->rx.push_back(discoveryAnswer(0xFFE9B194UL));
    ctrl.poll();
    TEST_ASSERT_EQUAL_size_t(1, bus->sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x02, commandOf(bus->sent[0]));
    const uint8_t bind[6] = {0xFF, 0xE9, 0xB1, 0x94, 0x00, 0x05};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(bind, bus->sent[0].data, 6);
    TEST_ASSERT_EQUAL_HEX16(0, mfx.locIdOf(0xFFE9B194UL)); // not confirmed yet

    // Echo of our Bind, then Verify after 20 ms.
    bus->rx.push_back(mfxFrame(0x02, true, 0x3363, 0xFFE9B194UL, 5));
    g_millis += 25;
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX8(0x03, commandOf(bus->sent.back()));

    // Verify answer with SID 0 (seen on the layout): ignored.
    bus->rx.push_back(mfxFrame(0x03, true, 0x3363, 0xFFE9B194UL, 0, 1));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX16(0, mfx.locIdOf(0xFFE9B194UL));

    // Verify answer with SID 5: confirmed.
    bus->rx.push_back(mfxFrame(0x03, true, 0x3363, 0xFFE9B194UL, 5, 1));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX16(0x4005, mfx.locIdOf(0xFFE9B194UL));

    // The loco announces itself again: same SID.
    bus->sent.clear();
    bus->rx.push_back(discoveryAnswer(0xFFE9B194UL));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX8(0x02, commandOf(bus->sent.back()));
    TEST_ASSERT_EQUAL_HEX8(0x05, bus->sent.back().data[5]);

    // A second loco gets SID 6.
    bus->rx.push_back(discoveryAnswer(0x11223344UL));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX8(0x06, bus->sent.back().data[5]);
}

void test_mfx_master_checks_known_locos_without_bind()
{
    TrackController ctrl(*bus, 0xDF24);
    MfxRegistry mfx(ctrl, MfxRegistry::Mode::Master);
    ctrl.setMfxRegistry(&mfx); // without the start sequence
    TEST_ASSERT_TRUE(mfx.assign(0xAAAA0001UL, 5));
    TEST_ASSERT_TRUE(mfx.assign(0xAAAA0002UL, 6));
    TEST_ASSERT_FALSE(mfx.busy());

    // As after a restart: both are checked again, one at a time.
    TEST_ASSERT_TRUE(mfx.verifyKnown());
    TEST_ASSERT_TRUE(mfx.busy());
    TEST_ASSERT_EQUAL_HEX16(0, mfx.locIdOf(0xAAAA0001UL));
    bus->sent.clear();
    ctrl.poll();
    TEST_ASSERT_EQUAL_size_t(1, bus->sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x03, commandOf(bus->sent[0])); // Verify, no Bind
    TEST_ASSERT_EQUAL_HEX8(0x05, bus->sent[0].data[5]);

    // The first loco answers: confirmed with its SID.
    bus->rx.push_back(mfxFrame(0x03, true, 0x3363, 0xAAAA0001UL, 5, 1));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX16(0x4005, mfx.locIdOf(0xAAAA0001UL));

    // The second one never answers: 3 Verify, then unconfirmed, then discovery.
    for (int i = 0; i < 10; i++)
    {
        ctrl.poll();
        g_millis += MfxRegistry::VERIFY_TIMEOUT_MS;
    }
    ctrl.poll();
    TEST_ASSERT_EQUAL_INT(0, bus->countSent(0x02)); // never a Bind
    TEST_ASSERT_EQUAL_INT(4, bus->countSent(0x03)); // 1 + 3 tries
    TEST_ASSERT_EQUAL_INT(1, bus->countSent(0x01)); // discovery after the checks
    TEST_ASSERT_EQUAL_HEX16(0, mfx.locIdOf(0xAAAA0002UL));
    TEST_ASSERT_EQUAL_STRING("unconfirmed", MfxRegistry::stateName(mfx.entry(1).state));
    TEST_ASSERT_FALSE(mfx.busy());

    // Later the loco is back on the track: it is checked again and answers.
    g_millis += MfxRegistry::RECHECK_INTERVAL_MS;
    bus->sent.clear();
    ctrl.poll();
    ctrl.poll();
    TEST_ASSERT_EQUAL_INT(1, bus->countSent(0x03));
    TEST_ASSERT_EQUAL_HEX8(0x06, bus->sent.back().data[5]);
    bus->rx.push_back(mfxFrame(0x03, true, 0x3363, 0xAAAA0002UL, 6, 1));
    ctrl.poll();
    TEST_ASSERT_EQUAL_HEX16(0x4006, mfx.locIdOf(0xAAAA0002UL));
}

void test_mfx_master_clear_releases_and_reregistration_cuts_power()
{
    TrackController ctrl(*bus, 0xDF24);
    MfxRegistry mfx(ctrl, MfxRegistry::Mode::Master);
    ctrl.setMfxRegistry(&mfx); // without the start sequence
    TEST_ASSERT_TRUE(mfx.assign(0xAAAA0001UL, 5));
    TEST_ASSERT_TRUE(mfx.assign(0xAAAA0002UL, 7));

    // clear(): Verify with UID 0 for SIDs 1..64 (also the locos no longer
    // in the table), then a discovery.
    bus->sent.clear();
    mfx.clear();
    TEST_ASSERT_EQUAL_size_t(MfxRegistry::RELEASE_SIDS + 1, bus->sent.size());
    TEST_ASSERT_EQUAL_INT(MfxRegistry::RELEASE_SIDS, bus->countSent(0x03));
    const uint8_t release1[6] = {0, 0, 0, 0, 0x00, 0x01};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(release1, bus->sent[0].data, 6);
    TEST_ASSERT_EQUAL_HEX8(0x07, bus->sent[6].data[5]);
    TEST_ASSERT_EQUAL_HEX8(0x01, commandOf(bus->sent.back()));
    TEST_ASSERT_EQUAL_UINT8(0, mfx.size());

    // forceReRegistration(): Stop, counter + 1, Go, discovery.
    bus->sent.clear();
    TEST_ASSERT_TRUE(mfx.forceReRegistration());
    TEST_ASSERT_EQUAL_size_t(4, bus->sent.size());
    TEST_ASSERT_EQUAL_HEX8(0x00, bus->sent[0].data[4]); // Stop
    TEST_ASSERT_EQUAL_HEX8(0x09, bus->sent[1].data[4]); // counter
    TEST_ASSERT_EQUAL_HEX8(0x02, bus->sent[1].data[6]); // 1 -> 2
    TEST_ASSERT_EQUAL_HEX8(0x01, bus->sent[2].data[4]); // Go
    TEST_ASSERT_EQUAL_HEX8(0x01, commandOf(bus->sent[3])); // discovery

    // Observer mode: clear() sends nothing.
    MfxRegistry observer(ctrl);
    TEST_ASSERT_TRUE(observer.assign(0xAAAA0003UL, 9));
    bus->sent.clear();
    observer.clear();
    TEST_ASSERT_TRUE(bus->sent.empty());
    TEST_ASSERT_FALSE(observer.release(9));
}

void test_mfx_master_gives_up_after_3_tries()
{
    TrackController ctrl(*bus, 0xDF24);
    MfxRegistry mfx(ctrl, MfxRegistry::Mode::Master);
    ctrl.setMfxRegistry(&mfx); // without the start sequence
    bus->rx.push_back(discoveryAnswer(0xFFE9B194UL));
    for (int i = 0; i < 20; i++)
    {
        ctrl.poll();
        g_millis += 600;
    }
    TEST_ASSERT_EQUAL_INT(3, bus->countSent(0x02));
    TEST_ASSERT_EQUAL_INT(3, bus->countSent(0x03));
    TEST_ASSERT_EQUAL_STRING("FAILED", MfxRegistry::stateName(mfx.entry(0).state));
}

void test_set_power_no_longer_sends_the_mfx_counter()
{
    TrackController ctrl(*bus, 0xDF24, false, 10);
    bus->onSend = [](const CanFrame &f) {
        CanFrame a = f;
        a.id |= (1UL << 16);
        bus->rx.push_back(a);
    };
    TEST_ASSERT_TRUE(ctrl.setPower(true));
    for (const CanFrame &f : bus->sent)
        TEST_ASSERT_FALSE(commandOf(f) == 0x00 && f.data[4] == 0x09);

    bus->sent.clear();
    ctrl.setMfxCounterOnPowerOn(true);
    TEST_ASSERT_TRUE(ctrl.setPower(true));
    bool counter = false;
    for (const CanFrame &f : bus->sent)
        counter = counter || (commandOf(f) == 0x00 && f.data[4] == 0x09 && f.data[6] == 0x03);
    TEST_ASSERT_TRUE(counter);
}

void test_bind_answer_for_another_uid_is_ignored()
{
    TrackController ctrl(*bus, 0xDF24, false, 50);
    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) != 0x02)
            return;
        bus->rx.push_back(mfxFrame(0x02, true, 0x3363, 0x11223344UL, 7)); // other loco
    };
    TrackMessage m = TrackMessage::make(Marklin::Cmd::Bind, 0xFFE9B194UL, 6);
    m.data[5] = 7;
    TEST_ASSERT_FALSE(ctrl.exchangeMessage(m, m, 50));

    bus->onSend = [](const CanFrame &f) {
        if (commandOf(f) == 0x02)
            bus->rx.push_back(mfxFrame(0x02, true, 0x3363, 0xFFE9B194UL, 7));
    };
    m = TrackMessage::make(Marklin::Cmd::Bind, 0xFFE9B194UL, 6);
    m.data[5] = 7;
    TEST_ASSERT_TRUE(ctrl.exchangeMessage(m, m, 50));
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
    RUN_TEST(test_answer_must_match_function_cv_and_length);
    RUN_TEST(test_system_sub_command_must_match);
    RUN_TEST(test_ms2_ping_between_request_and_answer);
    RUN_TEST(test_timeout_across_millis_wrap_around);
    RUN_TEST(test_send_error_returns_false);
    RUN_TEST(test_get_version_reads_the_gleisbox);
    RUN_TEST(test_begin_sends_the_wake_up_once_without_waiting);
    RUN_TEST(test_track_message_print_parse_round_trip);
    RUN_TEST(test_accessory_returns_the_real_result);
    RUN_TEST(test_async_accessory_switches_off_in_poll);
    RUN_TEST(test_async_accessory_switched_again_turns_previous_coil_off);
    RUN_TEST(test_switch_off_is_retried_then_given_up);
    RUN_TEST(test_accessory_command_line_reads_the_power);
    RUN_TEST(test_s88_with_32_modules);
    RUN_TEST(test_message_builder_and_uid);
    RUN_TEST(test_poll_passes_ms2_commands_to_the_handler);
    RUN_TEST(test_messages_received_while_waiting_are_not_lost);
    RUN_TEST(test_log_goes_to_the_chosen_output);
    RUN_TEST(test_direction_enum);
    RUN_TEST(test_command_parser);
    RUN_TEST(test_command_parser_reports_no_answer);
    RUN_TEST(test_mfx_observer_learns_from_the_ms2);
    RUN_TEST(test_mfx_assign_seeds_the_table);
    RUN_TEST(test_mfx_observer_ignores_discovery_and_verify_sid_0);
    RUN_TEST(test_mfx_master_discovery_bind_verify);
    RUN_TEST(test_mfx_master_gives_up_after_3_tries);
    RUN_TEST(test_mfx_master_checks_known_locos_without_bind);
    RUN_TEST(test_mfx_master_clear_releases_and_reregistration_cuts_power);
    RUN_TEST(test_set_power_no_longer_sends_the_mfx_counter);
    RUN_TEST(test_bind_answer_for_another_uid_is_ignored);
    RUN_TEST(test_mfx_list_is_sorted_by_sid);
    RUN_TEST(test_read_config_refused_at_once);
    RUN_TEST(test_config_cv_number_and_index);
    RUN_TEST(test_write_config_main_track_and_result);
    return UNITY_END();
}
