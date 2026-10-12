// Minimal host-side Arduino API used to compile and test Railuino on a PC.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <string>
#include <cstdio>

typedef uint8_t byte;
typedef bool boolean;

#define HEX 16
#define DEC 10
#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0
static const uint8_t A0 = 36;

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))

// ---- time -----------------------------------------------------------
extern uint32_t g_millis;      // simulated clock
extern uint32_t g_millisStep;  // advance on each millis() call
inline uint32_t millis() { uint32_t t = g_millis; g_millis += g_millisStep; return t; }
inline void delay(uint32_t ms) { g_millis += ms; }
inline void delayMicroseconds(unsigned int) {}
inline void yield() {}
inline long random(long max) { return std::rand() % max; }

// ---- GPIO -----------------------------------------------------------
extern int g_digitalReads;
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t, uint8_t) {}
inline int digitalRead(uint8_t) { return (g_digitalReads++ % 3) == 0; }

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))

// ---- String -----------------------------------------------------------
class String {
public:
    std::string s;
    String() {}
    String(const char *c) : s(c) {}
    String(const std::string &x) : s(x) {}
    String(unsigned long v, int base) {
        char buf[34];
        snprintf(buf, sizeof buf, base == 16 ? "%lx" : "%lu", v);
        s = buf;
    }
    unsigned int length() const { return (unsigned)s.size(); }
    char charAt(unsigned int i) const { return i < s.size() ? s[i] : 0; }
    String substring(unsigned int a) const { return a < s.size() ? String(s.substr(a)) : String(); }
    String substring(unsigned int a, unsigned int b) const {
        if (a >= s.size()) return String();
        return String(s.substr(a, b - a));
    }
    long toInt() const { return atol(s.c_str()); }
    bool startsWith(const char *p) const { return s.rfind(p, 0) == 0; }
    const char *c_str() const { return s.c_str(); }
    void trim() {
        size_t a = s.find_first_not_of(" \t\r\n");
        size_t b = s.find_last_not_of(" \t\r\n");
        s = (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);
    }
};

// ---- Print / Printable -----------------------------------------------------
class Print;
class Printable {
public:
    virtual ~Printable() {}
    virtual size_t printTo(Print &p) const = 0;
};

class Print {
public:
    std::string out;
    virtual ~Print() {}
    size_t print(const char *c) { out += c; return strlen(c); }
    size_t print(const String &x) { return print(x.c_str()); }
    size_t print(const __FlashStringHelper *f) { return print(reinterpret_cast<const char *>(f)); }
    size_t print(char c) { out += c; return 1; }
    size_t print(unsigned long v, int base = DEC) { return print(String(v, base)); }
    size_t print(long v, int base = DEC) { return v < 0 ? print("-") + print((unsigned long)-v, base) : print((unsigned long)v, base); }
    size_t print(int v, int base = DEC) { return print((long)v, base); }
    size_t print(unsigned int v, int base = DEC) { return print((unsigned long)v, base); }
    size_t print(unsigned char v, int base = DEC) { return print((unsigned long)v, base); }
    size_t print(const Printable &p) { return p.printTo(*this); }
    template <typename T> size_t println(T v) { size_t n = print(v); return n + print("\n"); }
    template <typename T> size_t println(T v, int base) { size_t n = print(v, base); return n + print("\n"); }
    size_t println() { return print("\n"); }
    // Serial-like helpers used by the examples.
    std::string in;
    void begin(unsigned long) {}
    explicit operator bool() const { return true; }
    int available() const { return (int)in.size(); }
    String readStringUntil(char c) {
        size_t p = in.find(c);
        String r(in.substr(0, p));
        in = (p == std::string::npos) ? std::string() : in.substr(p + 1);
        return r;
    }
};
extern Print Serial;
