#define _LINUX_ 1
#ifndef __BITBANG_I2C__
#define __BITBANG_I2C__
enum {
  DEVICE_UNKNOWN = 0,
  DEVICE_SSD1306,
  DEVICE_SH1106,
  DEVICE_VL53L0X,
  DEVICE_BMP180,
  DEVICE_BMP280,
  DEVICE_BME280,
  DEVICE_MPU6000,
  DEVICE_MPU9250,
  DEVICE_MCP9808,
  DEVICE_LSM6DS3,
  DEVICE_ADXL345,
  DEVICE_ADS1115,
  DEVICE_MAX44009,
  DEVICE_MAG3110,
  DEVICE_CCS811,
  DEVICE_HTS221,
  DEVICE_LPS25H,
  DEVICE_LSM9DS1,
  DEVICE_LM8330,
  DEVICE_DS3231,
  DEVICE_LIS3DH,
  DEVICE_LIS3DSH,
  DEVICE_INA219,
  DEVICE_SHT3X,
  DEVICE_HDC1080,
  DEVICE_MPU6886,
  DEVICE_BME680,
  DEVICE_AXP202,
  DEVICE_AXP192,
  DEVICE_24AAXXXE64,
  DEVICE_DS1307,
  DEVICE_MPU688X,
  DEVICE_FT6236G,
  DEVICE_FT6336G,
  DEVICE_FT6336U,
  DEVICE_FT6436,
  DEVICE_BM8563,
  DEVICE_BNO055
};
typedef struct mybbi2c {
  uint8_t iSDA, iSCL;
  uint8_t bWire;
  uint8_t iSDABit, iSCLBit;
  uint32_t iDelay;
#ifdef _LINUX_
  int file_i2c;
  int iBus;
#else
  volatile uint32_t *pSDADDR, *pSDAPORT;
  volatile uint32_t *pSCLDDR, *pSCLPORT;
#endif
} BBI2C;
int I2CRead(BBI2C *pI2C, uint8_t iAddr, uint8_t *pData, int iLen);
int I2CReadRegister(BBI2C *pI2C, uint8_t iAddr, uint8_t u8Register,
                    uint8_t *pData, int iLen);
int I2CWrite(BBI2C *pI2C, uint8_t iAddr, uint8_t *pData, int iLen);
uint8_t I2CTest(BBI2C *pI2C, uint8_t addr);
void I2CScan(BBI2C *pI2C, uint8_t *pMap);
void I2CInit(BBI2C *pI2C, uint32_t iClock);
int I2CDiscoverDevice(BBI2C *pI2C, uint8_t i);
#endif
