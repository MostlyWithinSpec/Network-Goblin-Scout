// Minimal stand-in for <Arduino.h> so the pure UI/core code builds on a PC.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

class String : public std::string {
 public:
  String(const char* s = "") : std::string(s) {}
  String(const std::string& s) : std::string(s) {}
  bool isEmpty() const { return empty(); }
};
