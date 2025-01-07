#define _LINUX_ 1
#ifdef _LINUX_
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#define PROGMEM
#define false 0
#define true 1
#define memcpy_P memcpy
#define INPUT 1
#define OUTPUT 2
const int iRPIPins[] = {-1, -1, -1, 2,  -1, 3,  -1, 4,  14, -1, 15, 17, 18, 27,
                        -1, 22, 23, -1, 24, 10, -1, 9,  25, 11, 8,  -1, 7,  0,
                        1,  5,  -1, 6,  12, 13, -1, 19, 16, 26, 20, -1, 21};
#else
#include <Arduino.h>
#ifndef __AVR_ATtiny85__
#include <Wire.h>
#endif
#ifdef W600_EV
#include <W600FastIO.h>
#define VARIANT_MCK 80000000ul
#endif
#endif
#include "BitBang_I2C.h"
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
volatile uint8_t *iDDR_SCL, *iPort_SCL_Out;
volatile uint8_t *iDDR_SDA, *iPort_SDA_In, *iPort_SDA_Out;
uint8_t iSDABit, iSCLBit;
#endif
#ifdef FUTURE
volatile uint32_t *iDDR_SCL, *iPort_SCL_Out;
volatile uint32_t *iDDR_SDA, *iPort_SDA_In, *iPort_SDA_Out;
uint32_t iSDABit, iSCLBit;
#endif
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
uint8_t getPinInfo(uint8_t pin, volatile uint8_t **iDDR,
                   volatile uint8_t **iPort, int bInput) {
  uint8_t port, bit;
  port = (pin & 0xf0);
  bit = pin & 0x7;
  switch (port) {
#ifdef PORTE
  case 0xE0:
    *iPort = (bInput) ? &PINE : &PORTE;
    *iDDR = &DDRE;
    break;
#endif
#ifdef PORTF
  case 0xF0:
    *iPort = (bInput) ? &PINF : &PORTF;
    *iDDR = &DDRF;
    break;
#endif
#ifdef PORTG
  case 0xA0:
    *iPort = (bInput) ? &PING : &PORTG;
    *iDDR = &DDRG;
    break;
#endif
#ifdef PORTC
  case 0xC0:
    *iPort = (bInput) ? &PINC : &PORTC;
    *iDDR = &DDRC;
    break;
#endif
#ifdef PORTB
  case 0xB0:
    *iPort = (bInput) ? &PINB : &PORTB;
    *iDDR = &DDRB;
    break;
#endif
#ifdef PORTD
  case 0xD0:
    *iPort = (bInput) ? &PIND : &PORTD;
    *iDDR = &DDRD;
    break;
#endif
  }
  return bit;
}
#endif
#ifdef FUTURE
uint32_t getPinInfo(uint8_t pin, volatile uint32_t **iDDR,
                    volatile uint32_t **iPort, int bInput) {
  uint32_t port, bit;
  if (pin <= 0xbf) {
    *iPort = (bInput) ? &REG_PORT_IN0 : &REG_PORT_OUT0;
    *iDDR = &REG_PORT_DIR0;
  } else if (pin <= 0xdf) {
    *iPort = (bInput) ? &REG_PORT_IN1 : &REG_PORT_OUT1;
    *iDDR = &REG_PORT_DIR1;
  } else
    return 0xffffffff;
  bit = pin & 0x1f;
  return bit;
}
#endif
inline uint8_t SDA_READ(uint8_t iSDA) {
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
  if (iSDA >= 0xa0) {
    if (*iPort_SDA_In & iSDABit)
      return HIGH;
    else
      return LOW;
  } else
#endif
  {
#ifndef __AVR_ATtiny85__
#ifdef W600_EV
    return w600DigitalRead(iSDA);
#else
#ifdef _LINUX_
    return 1;
#else
    return digitalRead(iSDA);
#endif
#endif
#endif
  }
  return 0;
}
inline void SCL_HIGH(uint8_t iSCL) {
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
  if (iSCL >= 0xa0) {
    *iDDR_SCL &= ~iSCLBit;
  } else
#endif
  {
#ifndef __AVR_ATtiny85__
#ifdef W600_EV
    w600PinMode(iSCL, GPIO_INPUT);
#else
#ifdef _LINUX_
#else
    pinMode(iSCL, INPUT);
#endif
#endif
#endif
  }
}
inline void SCL_LOW(uint8_t iSCL) {
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
  if (iSCL >= 0xa0) {
    *iDDR_SCL |= iSCLBit;
  } else
#endif
  {
#ifndef __AVR_ATtiny85__
#ifdef W600_EV
    w600PinMode(iSCL, GPIO_OUTPUT);
    w600DigitalWrite(iSCL, LOW);
#else
#ifdef _LINUX_
#else
    pinMode(iSCL, OUTPUT);
#endif
#endif
#endif
  }
}
inline void SDA_HIGH(uint8_t iSDA) {
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
  if (iSDA >= 0xa0) {
    *iDDR_SDA &= ~iSDABit;
  } else
#endif
  {
#ifndef __AVR_ATtiny85__
#ifdef W600_EV
    w600PinMode(iSDA, GPIO_INPUT);
#else
#ifdef _LINUX_
#else
    pinMode(iSDA, INPUT);
#endif
#endif
#endif
  }
}
inline void SDA_LOW(uint8_t iSDA) {
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
  if (iSDA >= 0xa0) {
    *iDDR_SDA |= iSDABit;
  } else
#endif
  {
#ifndef __AVR_ATtiny85__
#ifdef W600_EV
    w600PinMode(iSDA, GPIO_OUTPUT);
    w600DigitalWrite(iSDA, LOW);
#else
#ifdef _LINUX_
#else
    pinMode(iSDA, OUTPUT);
#endif
#endif
#endif
  }
}
void inline my_sleep_us(int iDelay) {
#ifdef __AVR_ATtiny85__
  iDelay *= 2;
  while (iDelay) {
    __asm__ __volatile__("nop"
                         "\n\t"
                         "nop");
    iDelay--;
  }
#else
  if (iDelay > 0)
#ifdef _LINUX_
    ;
#else
    delayMicroseconds(iDelay);
#endif
#endif
}
#ifndef __AVR_ATtiny85__
static inline int i2cByteOut(BBI2C *pI2C, uint8_t b) {
  uint8_t i, ack;
  uint8_t iSDA = pI2C->iSDA;
  uint8_t iSCL = pI2C->iSCL;
  int iDelay = pI2C->iDelay;
  for (i = 0; i < 8; i++) {
    if (b & 0x80)
      SDA_HIGH(iSDA);
    else
      SDA_LOW(iSDA);
    SCL_HIGH(iSCL);
    my_sleep_us(iDelay);
    SCL_LOW(iSCL);
    b <<= 1;
    my_sleep_us(iDelay);
  }
  SDA_HIGH(iSDA);
  SCL_HIGH(iSCL);
  my_sleep_us(iDelay);
  ack = SDA_READ(iSDA);
  SCL_LOW(iSCL);
  my_sleep_us(iDelay);
  SDA_LOW(iSDA);
  return (ack == 0) ? 1 : 0;
}
#endif
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
#define SDA_LOW_AVR *iDDR_sda |= sdabit;
#define SDA_HIGH_AVR *iDDR_sda &= ~sdabit;
#define SCL_LOW_AVR *iDDR_scl |= sclbit;
#define SCL_HIGH_AVR *iDDR_scl &= ~sclbit;
#define SDA_READ_AVR (*iPort_SDA_In & sdabit)
static inline int i2cByteOutAVR(BBI2C *pI2C, uint8_t b) {
  uint8_t i, ack;
  uint8_t *iDDR_sda = (uint8_t *)iDDR_SDA;
  uint8_t *iDDR_scl = (uint8_t *)iDDR_SCL;
  uint8_t sdabit = iSDABit;
  uint8_t sclbit = iSCLBit;
  for (i = 0; i < 8; i++) {
    if (b & 0x80)
      SDA_HIGH_AVR
    else
      SDA_LOW_AVR
    SCL_HIGH_AVR
    my_sleep_us(pI2C->iDelay);
    SCL_LOW_AVR
    b <<= 1;
  }
  SDA_HIGH_AVR
  SCL_HIGH_AVR
  ack = SDA_READ_AVR;
  SCL_LOW_AVR
  SDA_LOW_AVR
  return (ack == 0) ? 1 : 0;
}
#define BOTH_LOW_FAST *iDDR = both_low;
#define BOTH_HIGH_FAST *iDDR = both_high;
#define SCL_HIGH_FAST *iDDR = scl_high;
#define SDA_HIGH_FAST *iDDR = sda_high;
#define SDA_READ_FAST *iDDR &iSDABit;
static inline int i2cByteOutAVRFast(BBI2C *pI2C, uint8_t b) {
  uint8_t i, ack;
  uint8_t *iDDR = (uint8_t *)iDDR_SDA;
  uint8_t bOld = *iDDR;
  uint8_t both_low = bOld | iSDABit | iSCLBit;
  uint8_t both_high = bOld & ~(iSDABit | iSCLBit);
  uint8_t scl_high = (bOld | iSDABit) & ~iSCLBit;
  uint8_t sda_high = (bOld | iSCLBit) & ~iSDABit;
  BOTH_LOW_FAST
  for (i = 0; i < 8; i++) {
    if (b & 0x80) {
      SDA_HIGH_FAST
      my_sleep_us(pI2C->iDelay);
      BOTH_HIGH_FAST
    } else {
      SCL_HIGH_FAST
    }
    my_sleep_us(pI2C->iDelay);
    BOTH_LOW_FAST
    b <<= 1;
  }
  SDA_HIGH_FAST
  BOTH_HIGH_FAST
  my_sleep_us(pI2C->iDelay);
  ack = SDA_READ_FAST;
  BOTH_LOW_FAST
  return (ack == 0) ? 1 : 0;
}
#endif
#ifndef __AVR_ATtiny85__
static inline int i2cByteOutFast(BBI2C *pI2C, uint8_t b) {
  uint8_t i, ack, iSDA, iSCL;
  int iDelay;
  iSDA = pI2C->iSDA;
  iSCL = pI2C->iSCL;
  iDelay = pI2C->iDelay;
  if (b & 0x80)
    SDA_HIGH(iSDA);
  else
    SDA_LOW(iSDA);
  for (i = 0; i < 8; i++) {
    SCL_HIGH(iSCL);
    my_sleep_us(iDelay);
    SCL_LOW(iSCL);
    my_sleep_us(iDelay);
  }
  SDA_HIGH(iSDA);
  SCL_HIGH(iSCL);
  my_sleep_us(pI2C->iDelay);
  ack = SDA_READ(iSDA);
  SCL_LOW(iSCL);
  my_sleep_us(pI2C->iDelay);
  SDA_LOW(iSDA);
  return (ack == 0) ? 1 : 0;
}
#endif
static inline uint8_t i2cByteIn(BBI2C *pI2C, uint8_t bLast) {
  uint8_t i;
  uint8_t b = 0;
  SDA_HIGH(pI2C->iSDA);
  for (i = 0; i < 8; i++) {
    my_sleep_us(pI2C->iDelay);
    SCL_HIGH(pI2C->iSCL);
    b <<= 1;
    if (SDA_READ(pI2C->iSDA) != 0)
      b |= 1;
    SCL_LOW(pI2C->iSCL);
  }
  if (bLast)
    SDA_HIGH(pI2C->iSDA);
  else
    SDA_LOW(pI2C->iSDA);
  SCL_HIGH(pI2C->iSCL);
  my_sleep_us(pI2C->iDelay);
  SCL_LOW(pI2C->iSCL);
  my_sleep_us(pI2C->iDelay);
  SDA_LOW(pI2C->iSDA);
  return b;
}
static inline void i2cEnd(BBI2C *pI2C) {
  SDA_LOW(pI2C->iSDA);
  my_sleep_us(pI2C->iDelay);
  SCL_HIGH(pI2C->iSCL);
  my_sleep_us(pI2C->iDelay);
  SDA_HIGH(pI2C->iSDA);
  my_sleep_us(pI2C->iDelay);
}
static inline int i2cBegin(BBI2C *pI2C, uint8_t addr, uint8_t bRead) {
  int rc;
  SDA_LOW(pI2C->iSDA);
  my_sleep_us(pI2C->iDelay);
  SCL_LOW(pI2C->iSCL);
  addr <<= 1;
  if (bRead)
    addr++;
#ifdef __AVR_ATtiny85__
  rc = i2cByteOutAVR(pI2C, addr);
#else
  rc = i2cByteOut(pI2C, addr);
#endif
  return rc;
}
static inline int i2cWrite(BBI2C *pI2C, uint8_t *pData, int iLen) {
  uint8_t b;
  int rc, iOldLen = iLen;
  rc = 1;
  while (iLen && rc == 1) {
    b = *pData++;
#ifdef __AVR_ATtiny85__
    rc = i2cByteOutAVRFast(pI2C, b);
#else
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    if (pI2C->iSDA >= 0xa0) {
      rc = i2cByteOutAVRFast(pI2C, b);
    } else
#endif
    {
      if (b == 0xff || b == 0)
        rc = i2cByteOutFast(pI2C, b);
      else
        rc = i2cByteOut(pI2C, b);
    }
#endif
    if (rc == 1) {
      iLen--;
    }
  }
  return (rc == 1) ? (iOldLen - iLen) : 0;
}
static inline void i2cRead(BBI2C *pI2C, uint8_t *pData, int iLen) {
  while (iLen--) {
    *pData++ = i2cByteIn(pI2C, iLen == 0);
  }
}
void I2CInit(BBI2C *pI2C, uint32_t iClock) {
#ifdef _LINUX_
  {}
#endif
  if (pI2C == NULL)
    return;
  if (pI2C->bWire) {
#if !defined(_LINUX_) && !defined(__AVR_ATtiny85__)
#if defined(TEENSYDUINO) || defined(ARDUINO_ARCH_RP2040) ||                    \
    defined(__AVR__) || defined(NRF52) || defined(ARDUINO_ARCH_NRF52840) ||    \
    defined(ARDUINO_ARCH_NRF52) || defined(ARDUINO_ARCH_SAM)
    Wire.begin();
#else
    if (pI2C->iSDA == 0xff || pI2C->iSCL == 0xff)
      Wire.begin();
    else
      Wire.begin(pI2C->iSDA, pI2C->iSCL);
#endif
    Wire.setClock(iClock);
#endif
#ifdef _LINUX_
    {
      char filename[32];
      sprintf(filename, "/dev/i2c-%d", pI2C->iBus);
      if ((pI2C->file_i2c = open(filename, O_RDWR)) < 0)
        return;
    }
#endif
    return;
  }
  if (pI2C->iSDA < 0xa0) {
#if !defined(__AVR_ATtiny85__) && !defined(_LINUX_)
#ifdef W600_EV
    w600PinMode(pI2C->iSDA, GPIO_OUTPUT);
    w600PinMode(pI2C->iSCL, GPIO_OUTPUT);
    w600DigitalWrite(pI2C->iSDA, LOW);
    w600DigitalWrite(pI2C->iSCL, LOW);
    w600PinMode(pI2C->iSDA, GPIO_INPUT);
    w600PinMode(pI2C->iSCL, GPIO_INPUT);
#else
    pinMode(pI2C->iSDA, OUTPUT);
    pinMode(pI2C->iSCL, OUTPUT);
    digitalWrite(pI2C->iSDA, LOW);
    digitalWrite(pI2C->iSCL, LOW);
    pinMode(pI2C->iSDA, INPUT);
    pinMode(pI2C->iSCL, INPUT);
#endif
#endif
#ifdef _LINUX_
    pI2C->iSDA = iRPIPins[pI2C->iSDA];
    pI2C->iSCL = iRPIPins[pI2C->iSCL];
#endif
  }
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
  else {
    iSDABit = 1 << getPinInfo(pI2C->iSDA, &iDDR_SDA, &iPort_SDA_Out, 0);
    getPinInfo(pI2C->iSDA, &iDDR_SDA, &iPort_SDA_In, 1);
    iSCLBit = 1 << getPinInfo(pI2C->iSCL, &iDDR_SCL, &iPort_SCL_Out, 0);
    *iDDR_SDA &= ~iSDABit;
    *iDDR_SCL &= ~iSCLBit;
    *iPort_SDA_Out &= ~iSDABit;
    *iPort_SCL_Out &= ~iSCLBit;
  }
#endif
#ifdef _LINUX_
  pI2C->iDelay = 1000000 / iClock;
  if (pI2C->iDelay < 1)
    pI2C->iDelay = 1;
#else
  if (iClock >= 1000000)
    pI2C->iDelay = 0;
  else if (iClock >= 800000)
    pI2C->iDelay = 1;
  else if (iClock >= 400000)
    pI2C->iDelay = 2;
  else if (iClock >= 100000)
    pI2C->iDelay = 10;
  else
    pI2C->iDelay = (uint16_t)(1000000 / iClock);
#endif
}
uint8_t I2CTest(BBI2C *pI2C, uint8_t addr) {
  uint8_t response = 0;
  if (pI2C->bWire) {
#if !defined(_LINUX_) && !defined(__AVR_ATtiny85__)
    Wire.beginTransmission(addr);
    response = !Wire.endTransmission();
#endif
#ifdef _LINUX_
    if (ioctl(pI2C->file_i2c, I2C_SLAVE, addr) >= 0)
      response = 1;
#endif
    return response;
  }
  if (i2cBegin(pI2C, addr, 0)) {
    response = 1;
  }
  i2cEnd(pI2C);
  return response;
}
void I2CScan(BBI2C *pI2C, uint8_t *pMap) {
  int i;
  for (i = 0; i < 16; i++)
    pMap[i] = 0;
  for (i = 1; i < 128; i++) {
    if (I2CTest(pI2C, i)) {
      pMap[i >> 3] |= (1 << (i & 7));
    }
  }
}
int I2CWrite(BBI2C *pI2C, uint8_t iAddr, uint8_t *pData, int iLen) {
  int rc = 0;
  if (pI2C->bWire) {
#if !defined(_LINUX_) && !defined(__AVR_ATtiny85__)
    Wire.beginTransmission(iAddr);
    Wire.write(pData, (uint8_t)iLen);
    rc = !Wire.endTransmission();
#endif
#ifdef _LINUX_
    if (ioctl(pI2C->file_i2c, I2C_SLAVE, iAddr) >= 0) {
      if (write(pI2C->file_i2c, pData, iLen) >= 0)
        rc = 1;
    }
#endif
    return rc;
  }
  rc = i2cBegin(pI2C, iAddr, 0);
  if (rc == 1) {
    rc = i2cWrite(pI2C, pData, iLen);
  }
  i2cEnd(pI2C);
  return rc;
}
int I2CReadRegister(BBI2C *pI2C, uint8_t iAddr, uint8_t u8Register,
                    uint8_t *pData, int iLen) {
  int rc;
  if (pI2C->bWire) {
    int i = 0;
#if !defined(_LINUX_) && !defined(__AVR_ATtiny85__)
    Wire.beginTransmission(iAddr);
    Wire.write(u8Register);
    Wire.endTransmission();
    Wire.requestFrom(iAddr, (uint8_t)iLen);
    while (i < iLen) {
      pData[i++] = Wire.read();
    }
#endif
#ifdef _LINUX_
    if (ioctl(pI2C->file_i2c, I2C_SLAVE, iAddr) >= 0) {
      ssize_t bytesWritten = write(pI2C->file_i2c, &u8Register, 1);
      i = read(pI2C->file_i2c, pData, iLen);
    }
#endif
    return (i > 0);
  }
  rc = i2cBegin(pI2C, iAddr, 0);
  if (rc == 1) {
    rc = i2cWrite(pI2C, &u8Register, 1);
    if (rc == 1) {
      i2cEnd(pI2C);
      rc = i2cBegin(pI2C, iAddr, 1);
      if (rc == 1) {
        i2cRead(pI2C, pData, iLen);
      }
    }
  }
  i2cEnd(pI2C);
  return rc;
}
int I2CRead(BBI2C *pI2C, uint8_t iAddr, uint8_t *pData, int iLen) {
  int rc;
  if (pI2C->bWire) {
    int i = 0;
#if !defined(_LINUX_) && !defined(__AVR_ATtiny85__)
    Wire.requestFrom(iAddr, (uint8_t)iLen);
    while (i < iLen) {
      pData[i++] = Wire.read();
    }
#endif
#ifdef _LINUX_
    if (ioctl(pI2C->file_i2c, I2C_SLAVE, iAddr) >= 0) {
      i = read(pI2C->file_i2c, pData, iLen);
    }
#endif
    return (i > 0);
  }
  rc = i2cBegin(pI2C, iAddr, 1);
  if (rc == 1) {
    i2cRead(pI2C, pData, iLen);
  }
  i2cEnd(pI2C);
  return rc;
}
int I2CDiscoverDevice(BBI2C *pI2C, uint8_t i) {
  uint8_t j, cTemp[8];
  int iDevice = DEVICE_UNKNOWN;
  if (i == 0x28 || i == 0x29) {
    I2CReadRegister(pI2C, i, 0x00, cTemp, 1);
    if (cTemp[0] == 0xa0)
      return DEVICE_BNO055;
  }
  if (i == 0x3c || i == 0x3d) {
    I2CReadRegister(pI2C, i, 0x00, cTemp, 1);
    cTemp[0] &= 0xbf;
    if (cTemp[0] == 0x8)
      iDevice = DEVICE_SH1106;
    else if (cTemp[0] == 3 || cTemp[0] == 6)
      iDevice = DEVICE_SSD1306;
    return iDevice;
  }
  if (i == 0x34 || i == 0x35) {
    I2CReadRegister(pI2C, i, 0x03, cTemp, 1);
    if (cTemp[0] == 0x41)
      return DEVICE_AXP202;
    else if (cTemp[0] == 0x03)
      return DEVICE_AXP192;
  }
  if (i == 0x38) {
    I2CReadRegister(pI2C, i, 0xA0, cTemp, 1);
    if (cTemp[0] == 0x00)
      return DEVICE_FT6236G;
    else if (cTemp[0] == 0x01)
      return DEVICE_FT6336G;
    else if (cTemp[0] == 0x02)
      return DEVICE_FT6336U;
    else if (cTemp[0] == 0x03)
      return DEVICE_FT6436;
  }
  if (i >= 0x40 && i <= 0x4f) {
    I2CReadRegister(pI2C, i, 0x00, cTemp, 2);
    if (cTemp[0] == 0x39 && cTemp[1] == 0x9f)
      return DEVICE_INA219;
  }
  if (i >= 0x50 && i <= 0x57) {
    uint32_t u32Temp = 0;
    I2CReadRegister(pI2C, i, 0xf8, (uint8_t *)&u32Temp, 3);
    if (u32Temp == 0x000004a3 || u32Temp == 0x00001ec0 ||
        u32Temp == 0x00d88039 || u32Temp == 0x005410ec)
      return DEVICE_24AAXXXE64;
  }
  if (i == 0x51) {
    I2CReadRegister(pI2C, i, 0x00, cTemp, 1);
    if ((cTemp[0] & 0xDF) == 0x00) {
      I2CReadRegister(pI2C, i, 0x01, cTemp, 1);
      if ((cTemp[0] & 0xE0) == 0x00) {
        I2CReadRegister(pI2C, i, 0x02, cTemp, 1);
        if ((cTemp[0] & 0x80) == 0x00)
          return DEVICE_BM8563;
      }
    }
  }
  {
    I2CReadRegister(pI2C, i, 0xff, cTemp, 2);
    if (cTemp[0] == 0x10 && cTemp[1] == 0x50)
      return DEVICE_HDC1080;
    if (i == 0x76 || i == 0x77) {
      I2CReadRegister(pI2C, i, 0xd0, cTemp, 1);
      if (cTemp[0] == 0x61)
        return DEVICE_BME680;
    }
    I2CReadRegister(pI2C, i, 0xc0, cTemp, 3);
    if (cTemp[0] == 0xee && cTemp[1] == 0xaa && cTemp[2] == 0x10)
      return DEVICE_VL53L0X;
    I2CReadRegister(pI2C, i, 0x20, cTemp, 1);
    if (cTemp[0] == 0x81)
      return DEVICE_CCS811;
    I2CReadRegister(pI2C, i, 0x0f, cTemp, 1);
    if (cTemp[0] == 0x3f)
      return DEVICE_LIS3DSH;
    I2CReadRegister(pI2C, i, 0x0f, cTemp, 1);
    if (cTemp[0] == 0x33)
      return DEVICE_LIS3DH;
    I2CReadRegister(pI2C, i, 0x0f, cTemp, 1);
    if (cTemp[0] == 0x68)
      return DEVICE_LSM9DS1;
    I2CReadRegister(pI2C, i, 0x0f, cTemp, 1);
    if (cTemp[0] == 0xbd)
      return DEVICE_LPS25H;
    I2CReadRegister(pI2C, i, 0x0f, cTemp, 1);
    if (cTemp[0] == 0xbc)
      return DEVICE_HTS221;
    I2CReadRegister(pI2C, i, 0x07, cTemp, 1);
    if (cTemp[0] == 0xc4)
      return DEVICE_MAG3110;
    I2CReadRegister(pI2C, i, 0x80, cTemp, 2);
    if (cTemp[0] == 0x0 && cTemp[1] == 0x84)
      return DEVICE_LM8330;
    if (i == 0x4a || i == 0x4b) {
      for (j = 0; j < 8; j++)
        I2CReadRegister(pI2C, i, j, &cTemp[j], 1);
      if ((cTemp[2] == 3 || cTemp[2] == 2) && cTemp[6] == 0 && cTemp[7] == 0xff)
        return DEVICE_MAX44009;
    }
    I2CReadRegister(pI2C, i, 0x02, cTemp, 2);
    I2CReadRegister(pI2C, i, 0x03, &cTemp[2], 2);
    if (cTemp[0] == 0x80 && cTemp[1] == 0x00 && cTemp[2] == 0x7f &&
        cTemp[3] == 0xff)
      return DEVICE_ADS1115;
    I2CReadRegister(pI2C, i, 0x06, cTemp, 2);
    I2CReadRegister(pI2C, i, 0x07, &cTemp[2], 2);
    if (cTemp[0] == 0 && cTemp[1] == 0x54 && cTemp[2] == 0x04 &&
        cTemp[3] == 0x00)
      return DEVICE_MCP9808;
    I2CReadRegister(pI2C, i, 0xd0, cTemp, 1);
    if (cTemp[0] == 0x55)
      return DEVICE_BMP180;
    else if (cTemp[0] == 0x58)
      return DEVICE_BMP280;
    else if (cTemp[0] == 0x60)
      return DEVICE_BME280;
    I2CReadRegister(pI2C, i, 0x0f, cTemp, 1);
    if (cTemp[0] == 0x69)
      return DEVICE_LSM6DS3;
    I2CReadRegister(pI2C, i, 0x00, cTemp, 1);
    if (cTemp[0] == 0xe5)
      return DEVICE_ADXL345;
    if (i == 0x68 || i == 0x69) {
      I2CReadRegister(pI2C, i, 0x75, cTemp, 1);
      if (cTemp[0] == 0x19)
        return DEVICE_MPU6886;
    }
    I2CReadRegister(pI2C, i, 0x75, cTemp, 1);
    if (cTemp[0] == (i & 0xfe))
      return DEVICE_MPU6000;
    else if (cTemp[0] == 0x71)
      return DEVICE_MPU9250;
    else if (cTemp[0] == 0x19)
      return DEVICE_MPU688X;
    I2CReadRegister(pI2C, i, 0x0e, cTemp, 1);
    if (i == 0x68 && cTemp[0] == 0x1c)
      return DEVICE_DS3231;
    I2CReadRegister(pI2C, i, 0x07, cTemp, 1);
    if (i == 0x68 && cTemp[0] == 0x03)
      return DEVICE_DS1307;
  }
  return iDevice;
}
