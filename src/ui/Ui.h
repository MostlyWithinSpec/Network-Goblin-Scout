#pragma once
#include <Arduino.h>
#include "Companion.h"

class ScanManager;

namespace ui {
void begin(Companion* companion, ScanManager* scans);
void draw();                             // render current screen into the canvas + flush
void onTap(int16_t x, int16_t y);
void toast(const char* text, uint16_t color);
void splash(const char* line);
}  // namespace ui
