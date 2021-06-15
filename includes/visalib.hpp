#pragma once
#include <iostream>

#include "visa.h"
class visalib {
 public:
  visalib(char* deviceName);
  ~visalib();
  bool output(bool on);
  float readVoltage();
  float readCurrent();
  bool setVoltage(float voltage);

 private:
  ViSession defaultRM;
  ViSession instr;
};