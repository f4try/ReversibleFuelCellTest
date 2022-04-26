#include "seriallib.hpp"

seriallib::seriallib(char* portName) : gszPort(portName) {
  openDevice();
  setRemote();
}
seriallib::~seriallib() {
  setLocal();
  CloseHandle(hComm);
}
int seriallib::writeBytes(const void* Buffer, const unsigned int NbBytes) {
  // Number of bytes written
  DWORD dwBytesWritten;
  // Write data
  if (!WriteFile(hComm, Buffer, NbBytes, &dwBytesWritten, NULL))
  // Error while writing, return -1
  {
    std::cout << dwBytesWritten << " 写串口失败!" << std::endl;
    return -1;
  }
  // Write operation successfull
  // std::cout << dwBytesWritten << std::endl;
  return dwBytesWritten;
}
int seriallib::readBytes(void* buffer, unsigned int maxNbBytes) {
  // Number of bytes read
  DWORD dwBytesRead;

  // Read the bytes from the serial device, return -2 if an error occured
  if (!ReadFile(hComm, buffer, (DWORD)maxNbBytes, &dwBytesRead, NULL)) {
    std::cout << dwBytesRead << " 读取失败！" << std::endl;
    return -1;
  }

  // Return the byte read
  // std::cout << dwBytesRead << std::endl;
  return dwBytesRead;
}
bool seriallib::openDevice() {
  hComm = CreateFileA(gszPort, GENERIC_READ | GENERIC_WRITE, 0, 0,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (hComm == INVALID_HANDLE_VALUE) {
    std::cout << "failed to open serial port " << gszPort << " 打开串口"
              << gszPort << "失败!" << std::endl;
  }

  dcb.DCBlength = sizeof(dcb);
  if (!GetCommState(hComm, &dcb)) {
    std::cout << "获取串口通信状态失败！" << std::endl;
    return false;
  }

  dcb.BaudRate = 38400;
  dcb.ByteSize = 8;
  dcb.StopBits = 0;
  dcb.Parity = NOPARITY;
  if (!SetCommState(hComm, &dcb)) {
    std::cout << "串口通信设置失败！" << std::endl;
    return false;
  }
  SetupComm(hComm, 1000, 1000);

  // Set the Timeout parameters
  timeouts.ReadIntervalTimeout = 1000;
  // No TimeOut
  timeouts.ReadTotalTimeoutConstant = 5000;
  timeouts.ReadTotalTimeoutMultiplier = 500;
  timeouts.WriteTotalTimeoutConstant = 2000;
  timeouts.WriteTotalTimeoutMultiplier = 500;

  // Write the parameters
  if (!SetCommTimeouts(hComm, &timeouts)) {
    std::cout << "延时设置失败！" << std::endl;
    return false;
  }
  return true;
}

void seriallib::crc(unsigned char* Buffer) {
  *(Buffer + 25) = std::accumulate(Buffer, Buffer + 24, 0) % 256;
}

bool seriallib::readVCP(float* vcp) {
  const unsigned char inputBuffer[26] = {
      0xAA, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09};
  if (writeBytes(inputBuffer) != 26) {
    return false;
  }
  unsigned char outputBuffer[26];
  if (readBytes(outputBuffer) != 26) {
    return false;
  }
  vcp[0] = readHex(outputBuffer + 3) / 1000.0;
  vcp[1] = readHex(outputBuffer + 7) / 10000.0;
  vcp[2] = readHex(outputBuffer + 11) / 1000.0;
  return true;
}

int seriallib::readHex(const unsigned char* Buffer) {
  return Buffer[0] + Buffer[1] * 256 + Buffer[2] * 256 * 256 +
         Buffer[3] * 256 * 256 * 256;
}

bool seriallib::setRemote() {
  const unsigned char inputBuffer[26] = {
      0xAA, 0x00, 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCB};
  if (writeBytes(inputBuffer) != 26) {
    return false;
  }
  unsigned char outputBuffer[26];
  if (readBytes(outputBuffer) != 26) {
    return false;
  }
  return true;
}

bool seriallib::setLocal() {
  const unsigned char inputBuffer[26] = {
      0xAA, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCA};
  if (writeBytes(inputBuffer) != 26) {
    return false;
  }
  unsigned char outputBuffer[26];
  if (readBytes(outputBuffer) != 26) {
    return false;
  }
  return true;
}

bool seriallib::loadOn() {
  const unsigned char inputBuffer[26] = {
      0xAA, 0x00, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCC};
  if (writeBytes(inputBuffer) != 26) {
    return false;
  }
  unsigned char outputBuffer[26];
  if (readBytes(outputBuffer) != 26) {
    return false;
  }
  return true;
}
bool seriallib::loadOff() {
  const unsigned char inputBuffer[26] = {
      0xAA, 0x00, 0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xCB};
  if (writeBytes(inputBuffer) != 26) {
    return false;
  }
  unsigned char outputBuffer[26];
  if (readBytes(outputBuffer) != 26) {
    return false;
  }
  return true;
}

bool seriallib::setCurrent(float current) {
  unsigned char inputBuffer[26] = {0xAA, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00};
  const unsigned int current_int = (unsigned int)(current * 10000);
  inputBuffer[3] = current_int % 256;
  inputBuffer[4] = current_int >> 8 % 256;
  inputBuffer[5] = current_int >> 16 % 256;
  inputBuffer[6] = current_int >> 24 % 256;
  crc(inputBuffer);
  if (writeBytes(inputBuffer) != 26) {
    return false;
  }
  unsigned char outputBuffer[26];
  if (readBytes(outputBuffer) != 26) {
    return false;
  }
  return true;
}

bool seriallib::setVoltage(float voltage) {
  unsigned char inputBuffer[26] = {0xAA, 0x00, 0x2C, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00};
  const unsigned int voltage_int = (unsigned int)(voltage * 1000);
  inputBuffer[3] = voltage_int % 256;
  inputBuffer[4] = voltage_int >> 8 % 256;
  inputBuffer[5] = voltage_int >> 16 % 256;
  inputBuffer[6] = voltage_int >> 24 % 256;
  crc(inputBuffer);
  if (writeBytes(inputBuffer) != 26) {
    return false;
  }
  unsigned char outputBuffer[26];
  if (readBytes(outputBuffer) != 26) {
    return false;
  }
  return true;
}

bool seriallib::setLoadType(int load_type) {
  unsigned char inputBuffer[26] = {0xAA, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0xD2};
  if (load_type == 1) {
    inputBuffer[3] = 0x01;
    inputBuffer[25] = 0xD3;
  }
  if (writeBytes(inputBuffer) != 26) {
    return false;
  }
  unsigned char outputBuffer[26];
  if (readBytes(outputBuffer) != 26) {
    return false;
  }
  return true;
}