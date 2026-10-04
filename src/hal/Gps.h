#pragma once
#include <Arduino.h>

// Optional GPS on the P5 header. Auto-detected: if no NMEA arrives, present() stays false.
namespace gps {
void begin();
void poll();            // call often; drains UART into the NMEA parser
bool present();         // NMEA seen in the last few seconds
bool hasFix();
double lat();
double lon();
uint8_t satellites();
bool timeValid();
uint32_t unixTime();    // UTC seconds, 0 if unknown
}  // namespace gps
