#include "seriallib.hpp"
int main() {
  seriallib it8512("COM6");
  while (true) {
    std::vector<unsigned char> inputBuffer = {
        0xAA, 0x00, 0x5F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09};
    it8512.writeBytes(inputBuffer.data(), 26);
    Sleep(1000);
    std::vector<unsigned char> outputBuffer(26);
    it8512.readBytes(outputBuffer.data(), 26);
    for (int i = 0; i < 26; i++) {
      printf("%#x, ", outputBuffer[i]);
    }
    std::cout << std::endl;
    Sleep(1000);
  }
}