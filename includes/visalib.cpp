#include "visalib.hpp"

visalib::visalib(char* deviceName) {
  if (viOpenDefaultRM(&defaultRM) < VI_SUCCESS) {
    std::cout << "Could not open a session to the VISA Resource Manager!"
              << std::endl;
    return;
  }

  if (viOpen(defaultRM, deviceName, VI_NO_LOCK, 0, &instr) < VI_SUCCESS) {
    std::cout << "Cannot open a session to the device.<< std::endl"
              << std::endl;
    return;
  }

  viSetAttribute(instr, VI_ATTR_TMO_VALUE, 5000);
  viSetAttribute(instr, VI_ATTR_ASRL_BAUD, 9600);
  viSetAttribute(instr, VI_ATTR_ASRL_DATA_BITS, 8);
  viSetAttribute(instr, VI_ATTR_ASRL_PARITY, VI_ASRL_PAR_NONE);
  viSetAttribute(instr, VI_ATTR_ASRL_STOP_BITS, VI_ASRL_STOP_ONE);
  viSetAttribute(instr, VI_ATTR_TERMCHAR_EN, VI_TRUE);
  viSetAttribute(instr, VI_ATTR_TERMCHAR, 0xA);
}

visalib::~visalib() {
  viClose(instr);
  viClose(defaultRM);
}

bool visalib::output(bool on) {
  ViUInt32 count;
  if (on) {
    viWrite(instr, (ViBuf) "OUTP:TRIG 1\n", 12, &count);
  } else {
    viWrite(instr, (ViBuf) "OUTP:TRIG 0\n", 12, &count);
  }
  if (count != 12) {
    std::cout << "电源输出设置失败!" << count << std::endl;
    return false;
  }
  viWrite(instr, (ViBuf) "INIT:NAME OUTP\n", 15, &count);
  if (count != 15) {
    std::cout << "电源输出设置失败!" << count << std::endl;
    return false;
  }
  return true;
}

bool visalib::setVoltage(float voltage) {
  char buf[35];
  ViUInt32 count;
  sprintf(buf, "SOUR:VOLT:LEV:IMM:AMPL %.3f\n", voltage);
  ViUInt32 length = (ViUInt32)strlen(buf);
  viWrite(instr, (ViBuf)buf, length, &count);
  if (count != length) {
    std::cout << "设置电源电压失败!" << std::endl;
    return false;
  }
  return true;
}

float visalib::readVoltage() {
  ViUInt32 count;
  viWrite(instr, (ViBuf) "meas:volt:dc?\n", 14, &count);
  if (count != 14) {
    std::cout << "读取电源电压失败!" << count << std::endl;
    return false;
  }
  ViChar result[257];
  if (viRead(instr, (ViPBuf)result, 256, &count) < VI_SUCCESS) {
    std::cout << "读取电源电压失败!" << count << std::endl;
    return 0.0f;
  }
  return atof(result);
}

float visalib::readCurrent() {
  ViUInt32 count;
  viWrite(instr, (ViBuf) "meas:curr:dc?\n", 14, &count);
  if (count != 14) {
    std::cout << "读取电源电流失败!" << count << std::endl;
    return false;
  }
  ViChar result[257];
  if (viRead(instr, (ViPBuf)result, 256, &count) < VI_SUCCESS) {
    std::cout << "读取电源电流失败!" << count << std::endl;
    return 0.0f;
  }
  return atof(result);
}