/*
 * Railuino - Desktop Station serial gateway
 *
 * Original gateway by Yaasan (2013), ported to ESP32 by Uwe Monreal
 * (Serial_RS5_ESP32_LED, May 2026, pull request #1), adapted to the
 * Railuino library by Christophe Bobille.
 * License: CC0 1.0 (public domain), as the original gateway.
 *
 * Lets the Desktop Station software (https://desktopstation.net) drive a
 * Märklin layout (Gleisbox) through this board, connected over USB at
 * 115200 baud. Commands and answers follow Uwe Monreal's gateway, which
 * was tested with Desktop Station:
 *
 *   setPower(1)                 setLocoSpeed(16391,100)
 *   setLocoDirection(16391,1)   toggleLocoDirection(16391)
 *   setLocoFunction(16391,0,1)  toggleLocoFunction(16391,0)
 *   setTurnout(12289,1)         getVersion()
 *   getLocoConfig(16391,1)      setLocoConfig(16391,1,3)
 *   setPing()                   reset()
 *   getS88(1)                   (if S88_ENABLED)
 *   mfxDiscovery()              mfxBind(UID_HI,UID_LO,SID)   mfxVerify(...)
 *
 * Answers: 100 Ready, 200 Ok, 300 Command error, 301 Syntax error,
 * 302 receive timeout (incomplete line). Events from the bus: @PING,
 * @PWR, @DIR, @SPD, @FNC, @ACC, @MFX, @MFXBIND; values: @VER, @CV, @S88.
 *
 * Works on ESP32, Uno and Mega (MCP2515). Do not use an MfxRegistry
 * here: Desktop Station registers the MFX locomotives itself.
 */

#include <Railuino.h>

// CAN pins on ESP32 (ignored on Uno/Mega: MCP2515 on the SPI bus).
// Uwe Monreal's board used Rx 19 / Tx 22.
const uint8_t CAN_RX = 22;
const uint8_t CAN_TX = 23;

// S88 feedback (getS88). Disabled by default: choose pins that are free
// on your board. Uwe Monreal's ESP32 board used DATA 33, CLOCK 25,
// LOAD 23, RESET 21 — LOAD 23 conflicts with CAN_TX 23 above.
#define S88_ENABLED 0
const uint8_t S88_MODULES = 1; // default number of 16 bit modules
const uint8_t S88_DATA = 33;
const uint8_t S88_CLOCK = 25;
const uint8_t S88_LOAD = 26;
const uint8_t S88_RESET = 27;

const uint32_t SERIAL_BAUD = 115200;
const uint32_t EXCHANGE_TIMEOUT_MS = 500;
const uint32_t LINE_TIMEOUT_MS = 4000; // as the original gateway
const uint8_t LINE_CAPACITY = 128;
const uint8_t MAX_ARGS = 8;

TrackController ctrl(0, false, EXCHANGE_TIMEOUT_MS); // hash 0 = generated
#if S88_ENABLED
TrackReporterS88 reporter(S88_MODULES, S88_DATA, S88_CLOCK, S88_LOAD, S88_RESET);
#endif

char line[LINE_CAPACITY];
uint8_t lineLength = 0;
bool lineOverflow = false;
uint32_t lastCharMs = 0;
bool canReady = false;

// ---------------------------------------------------------------------
// Events from the bus (answers of the Gleisbox to other devices, pings)
// Same text as the original gateway: hexadecimal without leading zero.
// ---------------------------------------------------------------------

void printHexList(const uint8_t *data, uint8_t from, uint8_t to)
{
  for (uint8_t i = from; i <= to; i++)
  {
    Serial.print(data[i], HEX);
    Serial.print(',');
  }
}

void onCanMessage(const TrackMessage &m, void *)
{
  if (!m.response)
    return;

  switch (m.command)
  {
  case Marklin::Cmd::Ping:
    if (m.length == 8)
    {
      Serial.print(F("@PING,"));
      printHexList(m.data, 0, 7);
      Serial.println();
    }
    break;
  case Marklin::Cmd::System:
    if (m.length == 5)
    {
      Serial.print(F("@PWR,"));
      printHexList(m.data, 4, 4);
      Serial.println();
    }
    break;
  case Marklin::Cmd::Direction:
    if (m.length == 5)
    {
      Serial.print(F("@DIR,"));
      printHexList(m.data, 2, 4);
      Serial.println();
    }
    break;
  case Marklin::Cmd::Speed:
    if (m.length == 6)
    {
      Serial.print(F("@SPD,"));
      printHexList(m.data, 2, 5);
      Serial.println();
    }
    break;
  case Marklin::Cmd::Function:
    if (m.length == 6)
    {
      Serial.print(F("@FNC,"));
      printHexList(m.data, 2, 4);
      Serial.print(m.data[5]); // decimal, as the original
      Serial.println(',');
    }
    break;
  case Marklin::Cmd::Accessory:
    if (m.length == 6 && m.data[5] != 0)
    {
      Serial.print(F("@ACC,"));
      printHexList(m.data, 2, 5);
      Serial.println();
    }
    break;
  case Marklin::Cmd::Discovery:
    if (m.length == 5)
    {
      Serial.print(F("@MFX,"));
      printHexList(m.data, 0, 3);
      Serial.println();
    }
    break;
  case Marklin::Cmd::Bind:
    if (m.length == 6)
    {
      Serial.print(F("@MFXBIND,"));
      printHexList(m.data, 0, 5);
      Serial.println();
    }
    break;
  }
}

// ---------------------------------------------------------------------
// Commands: name(arg,arg,...) with decimal arguments
// ---------------------------------------------------------------------

// Decimal number 0..65535; spaces around it are allowed.
bool parseWord(char *s, uint16_t &value)
{
  while (*s == ' ' || *s == '\t')
    s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t'))
    *--end = 0;
  if (*s == 0)
    return false;
  uint32_t v = 0;
  for (; *s; s++)
  {
    if (*s < '0' || *s > '9')
      return false;
    v = v * 10 + (*s - '0');
    if (v > 65535)
      return false;
  }
  value = v;
  return true;
}

bool parseCommand(char *text, char *&name, uint16_t *args, uint8_t &count)
{
  char *lpar = strchr(text, '(');
  char *rpar = strrchr(text, ')');
  if (lpar == nullptr || rpar == nullptr || rpar < lpar)
    return false;
  *lpar = 0;
  *rpar = 0;

  name = text;
  while (*name == ' ' || *name == '\t')
    name++;
  char *end = name + strlen(name);
  while (end > name && (end[-1] == ' ' || end[-1] == '\t'))
    *--end = 0;
  if (*name == 0)
    return false;

  count = 0;
  char *p = lpar + 1;
  // Empty argument list: "getVersion()" or "getVersion( )".
  char *q = p;
  while (*q == ' ' || *q == '\t')
    q++;
  if (*q == 0)
    return true;

  while (true)
  {
    if (count >= MAX_ARGS)
      return false;
    char *comma = strchr(p, ',');
    if (comma != nullptr)
      *comma = 0;
    if (!parseWord(p, args[count++]))
      return false;
    if (comma == nullptr)
      return true;
    p = comma + 1;
  }
}

bool sendUidSid(uint8_t command, uint16_t uidHigh, uint16_t uidLow, uint16_t sid)
{
  // Uwe Monreal's gateway: first argument -> data[0..1], second ->
  // data[2..3], i.e. the UID in the order it is written (FFE9, B194).
  TrackMessage m = TrackMessage::make(command, (static_cast<uint32_t>(uidHigh) << 16) | uidLow, 6);
  m.data[4] = highByte(sid);
  m.data[5] = lowByte(sid);
  ctrl.exchangeMessage(m, m, 1000);
  return true; // the original always answers 200 Ok
}

bool readS88(uint16_t modules)
{
#if S88_ENABLED
  if (modules == 0)
    modules = S88_MODULES;
  if (modules > TrackReporterS88::MAX_MODULES)
    modules = TrackReporterS88::MAX_MODULES;
  reporter.refresh(modules);
  Serial.print(F("@S88,"));
  for (uint8_t j = 0; j < modules; j++)
  {
    const uint16_t flags = (static_cast<uint16_t>(reporter.getByte((j << 1) + 1)) << 8) | reporter.getByte(j << 1);
    Serial.print(flags, HEX);
    Serial.print(',');
  }
  Serial.println();
  return true;
#else
  (void)modules;
  return false; // S88 not enabled: 300 Command error
#endif
}

// Same results as the original gateway: false gives "300 Command error".
bool dispatch(const char *name, const uint16_t *a, uint8_t n)
{
  if (!strcmp(name, "setPower") && n == 1)
    return ctrl.setPower(a[0] != 0);
  if (!strcmp(name, "setLocoSpeed") && n == 2)
    return ctrl.setLocoSpeed(a[0], a[1]);
  if (!strcmp(name, "setLocoDirection") && n == 2)
    return ctrl.setLocoDirection(a[0], static_cast<uint8_t>(a[1]));
  if (!strcmp(name, "toggleLocoDirection") && n == 1)
    return ctrl.toggleLocoDirection(a[0]);
  if (!strcmp(name, "setLocoFunction") && n == 3)
    return ctrl.setLocoFunction(a[0], static_cast<uint8_t>(a[1]), static_cast<uint8_t>(a[2]));
  if (!strcmp(name, "toggleLocoFunction") && n == 2)
    return ctrl.toggleLocoFunction(a[0], static_cast<uint8_t>(a[1]));
  if (!strcmp(name, "setTurnout") && n == 2)
    return ctrl.setTurnout(a[0], a[1] != 0);
  if (!strcmp(name, "getVersion") && n == 0)
  {
    uint8_t high = 0, low = 0;
    const bool ok = ctrl.getVersion(&high, &low);
    Serial.print(F("@VER,"));
    Serial.print(high, HEX);
    Serial.print(low, HEX);
    Serial.println(',');
    return ok;
  }
  if (!strcmp(name, "getLocoConfig") && n == 2)
  {
    uint8_t value = 0;
    ctrl.readConfig(a[0], a[1], &value);
    Serial.print(F("@CV,"));
    Serial.print(a[0]);
    Serial.print(',');
    Serial.print(a[1]);
    Serial.print(',');
    Serial.print(value);
    Serial.println(',');
    return true; // as the original
  }
  if (!strcmp(name, "setLocoConfig") && n == 3)
    return ctrl.writeConfig(a[0], a[1], static_cast<uint8_t>(a[2]));
  if (!strcmp(name, "setPing") && n == 0)
  {
    TrackMessage m = TrackMessage::make(Marklin::Cmd::Ping, 0, 0);
    ctrl.sendMessage(m);
    return true;
  }
  if (!strcmp(name, "getS88") && n <= 1)
    return readS88(n == 1 ? a[0] : 0);
  if (!strcmp(name, "reset") && n == 0)
  {
    ctrl.end(); // empties the receive queue; the CAN controller keeps running
    Serial.println(F("100 Ready"));
    return true;
  }
  if (!strcmp(name, "mfxDiscovery") && n == 0)
  {
    TrackMessage m = TrackMessage::make(Marklin::Cmd::Discovery, 0, 1);
    m.data[0] = 0x20;
    ctrl.sendMessage(m);
    return true;
  }
  if (!strcmp(name, "mfxBind") && n == 3)
    return sendUidSid(Marklin::Cmd::Bind, a[0], a[1], a[2]);
  if (!strcmp(name, "mfxVerify") && n == 3)
    return sendUidSid(Marklin::Cmd::Verify, a[0], a[1], a[2]);
  return false;
}

void handleLine()
{
  char *name = nullptr;
  uint16_t args[MAX_ARGS] = {0};
  uint8_t count = 0;
  if (!parseCommand(line, name, args, count))
    Serial.println(F("301 Syntax error"));
  else if (dispatch(name, args, count))
    Serial.println(F("200 Ok"));
  else
    Serial.println(F("300 Command error"));
}

// ---------------------------------------------------------------------

void setup()
{
  Serial.begin(SERIAL_BAUD);
  canReady = ctrl.begin(CAN_RX, CAN_TX);
  ctrl.onMessage(onCanMessage); // after begin(): no events before "100 Ready"
  Serial.println(F("--------------------------------------"));
  Serial.println(F("Desktop Station Interface (Railuino)"));
  Serial.println(F("--------------------------------------"));
  if (!canReady)
  {
    Serial.println(F("303 Unknown error")); // CAN could not be started
    return;
  }
#if S88_ENABLED
  reporter.refresh();
#endif
  Serial.println(F("100 Ready"));
}

void loop()
{
  if (!canReady)
  {
    delay(1000);
    return;
  }

  while (Serial.available())
  {
    const char c = Serial.read();
    lastCharMs = millis();
    if (c == '\r')
      continue;
    if (c == '\n')
    {
      if (lineOverflow)
        Serial.println(F("301 Syntax error"));
      else if (lineLength > 0)
      {
        line[lineLength] = 0;
        handleLine();
      }
      lineLength = 0;
      lineOverflow = false;
    }
    else if (lineLength + 1 < LINE_CAPACITY)
      line[lineLength++] = c;
    else
      lineOverflow = true;
  }

  // Incomplete line for 4 s: same answer as the original gateway.
  if ((lineLength > 0 || lineOverflow) && millis() - lastCharMs > LINE_TIMEOUT_MS)
  {
    Serial.println(F("302 receive timeout"));
    lineLength = 0;
    lineOverflow = false;
  }

  ctrl.poll(); // bus events -> onCanMessage
}
