/*
 * Railuino / Desktop Station serial bridge for ESP32
 * Adapted from Serial_RS5_ESP32_LED by Uwe Monreal (May 2026),
 * based on Desktop Station gateway by Yaasan (2013).
 * This example is offered under CC0 1.0 as per the original gateway.
 *
 * This is a migration of the gateway, NOT a copy of old TrackController classes.
 * Hardware testing with Desktop Station and a Gleisbox is required.
 */
#include <Arduino.h>
#include <Railuino.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "DesktopStation gateway currently targets ESP32 only"
#endif

// Adjust for your CAN transceiver and ESP32 board.
static constexpr uint8_t CAN_RX = 22;
static constexpr uint8_t CAN_TX = 23;
static constexpr uint32_t SERIAL_BAUD = 115200;
static constexpr uint32_t EXCHANGE_TIMEOUT_MS = 500;
static constexpr size_t LINE_CAPACITY = 128;
static constexpr size_t MAX_ARGS = 8;

TrackController controller(0, false, false);
static char inputLine[LINE_CAPACITY] = {};
static size_t lineLength = 0;
static bool overflow = false;
static bool canReady = false;

static void reportError(const char *error) { Serial.println(error); }
static void printHexByte(uint8_t b) { Serial.print(b, HEX); }

// Protocol event notifications follow Uwe Monreal's @NAME,... syntax.
static void reportCanEvent(const TrackMessage &m) {
  if (!m.response) return;
  if (m.command == 0x18 && m.length == 8) {
    Serial.print(F("@PING,"));
    for (uint8_t i=0; i<8; ++i) { printHexByte(m.data[i]); Serial.print(','); }
    Serial.println();
  } else if (m.command == 0x00 && m.length >= 5) {
    Serial.print(F("@PWR,")); printHexByte(m.data[4]); Serial.println(',');
  } else if (m.command == 0x05 && m.length >= 5) {
    Serial.print(F("@DIR,")); printHexByte(m.data[2]); Serial.print(',');
    printHexByte(m.data[3]); Serial.print(','); printHexByte(m.data[4]); Serial.println(',');
  } else if (m.command == 0x04 && m.length >= 6) {
    Serial.print(F("@SPD,")); printHexByte(m.data[2]); Serial.print(',');
    printHexByte(m.data[3]); Serial.print(','); printHexByte(m.data[4]); Serial.print(',');
    printHexByte(m.data[5]); Serial.println(',');
  } else if (m.command == 0x06 && m.length >= 6) {
    Serial.print(F("@FNC,")); printHexByte(m.data[2]); Serial.print(',');
    printHexByte(m.data[3]); Serial.print(','); printHexByte(m.data[4]); Serial.print(',');
    Serial.print(m.data[5]); Serial.println(','); // decimal, as in Uwe's implementation
  } else if (m.command == 0x0B && m.length >= 6 && m.data[5] != 0) {
    Serial.print(F("@ACC,")); printHexByte(m.data[2]); Serial.print(',');
    printHexByte(m.data[3]); Serial.print(','); printHexByte(m.data[4]); Serial.print(',');
    printHexByte(m.data[5]); Serial.println(',');
  } else if (m.command == 0x01 && m.length == 5) {
    Serial.print(F("@MFX,"));
    for (uint8_t i=0; i<4; ++i) { printHexByte(m.data[i]); Serial.print(','); }
    Serial.println();
  } else if (m.command == 0x02 && m.length == 6) {
    Serial.print(F("@MFXBIND,"));
    for (uint8_t i=0; i<6; ++i) { printHexByte(m.data[i]); Serial.print(','); }
    Serial.println();
  }
}

// Strict decimal parser: prevents silent wraparound and numeric garbage.
static bool parseUint16(const char *s, uint16_t &result) {
  if (!s || !*s) return false;
  uint32_t value=0;
  for (; *s; ++s) {
    if (*s < '0' || *s > '9') return false;
    value = value*10 + static_cast<uint8_t>(*s-'0');
    if (value > 65535) return false;
  }
  result = static_cast<uint16_t>(value);
  return true;
}

// Parse "function(arg,arg)" in-place without dynamic String allocations.
static bool parseCommand(char *line, char *&name, uint16_t (&args)[MAX_ARGS], size_t &count) {
  char *lparen = strchr(line, '(');
  char *rparen = strrchr(line, ')');
  if (!lparen || !rparen || rparen < lparen || rparen[1] != '\0') return false;
  *lparen = '\0'; *rparen = '\0'; name = line;
  // trim spaces surrounding function name
  while (*name == ' ' || *name == '\t') ++name;
  char *end = name+strlen(name);
  while (end>name && (end[-1]==' ' || end[-1]=='\t')) --end;
  *end = '\0';
  if (!*name) return false;
  count=0;
  char *p=lparen+1;
  while (*p) {
    if (count >= MAX_ARGS) return false;
    char *comma = strchr(p, ',');
    if (comma) *comma='\0';
    while (*p == ' ' || *p == '\t') ++p;
    char *last=p+strlen(p);
    while (last>p && (last[-1]==' ' || last[-1]=='\t')) --last;
    *last='\0';
    if (!parseUint16(p,args[count++])) return false;
    if (!comma) break;
    p=comma+1;
    if (!*p) return false;
  }
  return true;
}

static bool sendMfxCommand(uint8_t command, const uint16_t *args, bool waitReply) {
  TrackMessage m;
  m.clear(); m.command=command;
  if (command == 0x01) {
    m.length=1; m.data[0]=0x20;
    return controller.sendMessage(m);
  }
  m.length=6;
  // Original Desktop Station API: UID lower word, UID upper word, SID.
  m.data[0]=static_cast<uint8_t>(args[0] >> 8);
  m.data[1]=static_cast<uint8_t>(args[0]);
  m.data[2]=static_cast<uint8_t>(args[1] >> 8);
  m.data[3]=static_cast<uint8_t>(args[1]);
  m.data[4]=static_cast<uint8_t>(args[2] >> 8);
  m.data[5]=static_cast<uint8_t>(args[2]);
  return waitReply ? controller.exchangeMessage(m,m,1000) : controller.sendMessage(m);
}

static bool dispatch(const char *name, const uint16_t *a, size_t n, bool &supported) {
  supported=true;
  if (!strcmp(name,"setPower") && n==1 && a[0]<=1) return controller.setPower(a[0]!=0);
  if (!strcmp(name,"setLocoSpeed") && n==2) return controller.setLocoSpeed(a[0],a[1]);
  if (!strcmp(name,"setLocoDirection") && n==2 && a[1]<=3) return controller.setLocoDirection(a[0],static_cast<uint8_t>(a[1]));
  if (!strcmp(name,"toggleLocoDirection") && n==1) return controller.toggleLocoDirection(a[0]);
  if (!strcmp(name,"setLocoFunction") && n==3 && a[1]<=255 && a[2]<=1) return controller.setLocoFunction(a[0],static_cast<uint8_t>(a[1]),static_cast<uint8_t>(a[2]));
  if (!strcmp(name,"toggleLocoFunction") && n==2 && a[1]<=255) return controller.toggleLocoFunction(a[0],static_cast<uint8_t>(a[1]));
  if (!strcmp(name,"setTurnout") && n==2 && a[1]<=1) return controller.setTurnout(a[0],a[1]!=0);
  if (!strcmp(name,"getVersion") && n==0) {
    uint8_t high=0,low=0; const bool ok=controller.getVersion(&high,&low);
    if (ok) { Serial.print(F("@VER,")); printHexByte(high); printHexByte(low); Serial.println(','); }
    return ok;
  }
  if (!strcmp(name,"getLocoConfig") && n==2) {
    uint8_t value=0; const bool ok=controller.readConfig(a[0],a[1],&value);
    if (ok) { Serial.print(F("@CV,")); Serial.print(a[0]); Serial.print(','); Serial.print(a[1]); Serial.print(','); Serial.print(value); Serial.println(','); }
    return ok;
  }
  if (!strcmp(name,"setLocoConfig") && n==3 && a[2]<=255) return controller.writeConfig(a[0],a[1],static_cast<uint8_t>(a[2]));
  if (!strcmp(name,"setPing") && n==0) {
    TrackMessage m; m.clear(); m.command=0x18; return controller.sendMessage(m);
  }
  if (!strcmp(name,"mfxDiscovery") && n==0) return sendMfxCommand(0x01,a,false);
  if (!strcmp(name,"mfxBind") && n==3) return sendMfxCommand(0x02,a,true);
  if (!strcmp(name,"mfxVerify") && n==3) return sendMfxCommand(0x03,a,true);
  // getS88 and reset are intentionally NOT advertised as implemented.
  supported=false;
  return false;
}

static void handleLine(char *line) {
  char *name=nullptr;
  uint16_t args[MAX_ARGS]={}; size_t n=0;
  if (!parseCommand(line,name,args,n)) { reportError("301 Syntax error"); return; }
  bool supported=false;
  const bool ok=dispatch(name,args,n,supported);
  if (!supported) reportError("300 Command error");
  else if (ok) Serial.println(F("200 Ok"));
  else reportError("302 receive timeout"); // Includes CAN send failures; legacy Desktop Station reply.
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  controller.setTimeout(EXCHANGE_TIMEOUT_MS);
  canReady = controller.begin(CAN_RX,CAN_TX);
  if (!canReady) { Serial.println(F("303 CAN initialization error")); return; }
  Serial.println(F("100 Ready"));
}

void loop() {
  if (!canReady) { delay(1000); return; }
  while (Serial.available()) {
    const char c=static_cast<char>(Serial.read());
    if (c=='\r') continue;
    if (c=='\n') {
      if (overflow) reportError("301 Syntax error");
      else if (lineLength) { inputLine[lineLength]='\0'; handleLine(inputLine); }
      lineLength=0; overflow=false;
    } else if (!overflow) {
      if (lineLength+1<LINE_CAPACITY) inputLine[lineLength++]=c;
      else overflow=true;
    }
  }
  // Asynchronous CAN events, as used by Uwe's Desktop Station bridge.
  TrackMessage message;
  uint8_t drained=0;
  while (drained++<32 && controller.receiveMessage(message)) reportCanEvent(message);
  delay(1);
}
