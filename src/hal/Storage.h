#pragma once
#include <Arduino.h>
#include <SD.h>

namespace storage {
bool begin();
bool ok();
bool ensureDir(const char* path);
String readText(const char* path);
bool writeTextAtomic(const char* path, const String& text);
bool append(const char* path, const String& text);
bool readBytes(const char* path, uint8_t* buf, size_t len);
bool writeBytes(const char* path, const uint8_t* buf, size_t len);
}  // namespace storage
