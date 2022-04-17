#pragma once
#include <windows.h>

#include <iostream>
#include <numeric>
#include <vector>

class seriallib {
 public:
  seriallib(char* portName);
  ~seriallib();
  int writeBytes(const void* Buffer, const unsigned int NbBytes = 26);
  int readBytes(void* buffer, unsigned int maxNbBytes = 26);
  void crc(unsigned char* Buffer);
  bool readVCP(float* vcp);
  static int readHex(const unsigned char* Buffer);
  bool loadOn();
  bool loadOff();
  bool setCurrent(float current);
  bool setVoltage(float voltage);
  bool setLoadType(int load_type);

 private:
  LPCSTR gszPort;
  HANDLE hComm;
  DCB dcb = {0};
  COMMTIMEOUTS timeouts;
  bool openDevice();
  bool setRemote();
  bool setLocal();
};
