#ifndef __SS_OLED_H__
#define __SS_OLED_H__
#include "BitBang_I2C.h"
typedef struct ssoleds {
  uint8_t oled_addr;
  uint8_t oled_wrap, oled_flip, oled_type;
  uint8_t *ucScreen;
  uint8_t iCursorX, iCursorY;
  uint8_t oled_x, oled_y;
  int iScreenOffset;
  BBI2C bbi2c;
} SSOLED;
#if defined(_LINUX_) && defined(__cplusplus)
extern "C" {
#endif
#ifndef SPI_LCD_H
enum { FONT_6x8 = 0, FONT_8x8, FONT_12x16, FONT_16x16, FONT_16x32 };
enum { ROT_0 = 0, ROT_90, ROT_180, ROT_270 };
#define FONT_NORMAL FONT_8x8
#define FONT_SMALL FONT_6x8
#define FONT_LARGE FONT_16x32
#define FONT_STRETCHED FONT_16x16
#endif
enum {
  OLED_128x128 = 1,
  OLED_128x32,
  OLED_128x64,
  OLED_132x64,
  OLED_64x32,
  OLED_96x16,
  OLED_72x40
};
enum { ANGLE_0 = 0, ANGLE_90, ANGLE_180, ANGLE_270, ANGLE_FLIPX, ANGLE_FLIPY };
enum {
  OLED_NOT_FOUND = -1,
  OLED_SSD1306_3C,
  OLED_SSD1306_3D,
  OLED_SH1106_3C,
  OLED_SH1106_3D,
  OLED_SH1107_3C,
  OLED_SH1107_3D
};
int oledInit(SSOLED *pOLED, int iType, int iAddr, int bFlip, int bInvert,
             int bWire, int iSDAPin, int iSCLPin, int iResetPin,
             int32_t iSpeed);
void oledSPIInit(int iType, int iDC, int iCS, int iReset, int bFlip,
                 int bInvert, int32_t iSpeed);
void oledSetBackBuffer(SSOLED *pOLED, uint8_t *pBuffer);
void oledSetContrast(SSOLED *pOLED, unsigned char ucContrast);
int oledLoadBMP(SSOLED *pOLED, uint8_t *pBMP, int bInvert, int bRender);
void oledPower(SSOLED *pOLED, uint8_t bOn);
void oledSetCursor(SSOLED *pOLED, int x, int y);
void oledSetTextWrap(SSOLED *pOLED, int bWrap);
int oledWriteString(SSOLED *pOLED, int iScrollX, int x, int y, char *szMsg,
                    int iSize, int bInvert, int bRender);
int oledScaledString(SSOLED *pOLED, int x, int y, char *szMsg, int iSize,
                     int bInvert, int iXScale, int iYScale, int iRotation);
void oledFill(SSOLED *pOLED, unsigned char ucData, int bRender);
int oledSetPixel(SSOLED *pOLED, int x, int y, unsigned char ucColor,
                 int bRender);
void oledDumpBuffer(SSOLED *pOLED, uint8_t *pBuffer);
int oledDrawGFX(SSOLED *pOLED, uint8_t *pSrc, int iSrcCol, int iSrcRow,
                int iDestCol, int iDestRow, int iWidth, int iHeight,
                int iSrcPitch);
void oledDrawLine(SSOLED *pOLED, int x1, int y1, int x2, int y2, int bRender);
uint8_t *oledPlayAnimFrame(SSOLED *pOLED, uint8_t *pAnimation,
                           uint8_t *pCurrent, int iLen);
int oledScrollBuffer(SSOLED *pOLED, int iStartCol, int iEndCol, int iStartRow,
                     int iEndRow, int bUp);
void oledDrawSprite(SSOLED *pOLED, uint8_t *pSprite, int cx, int cy, int iPitch,
                    int x, int y, uint8_t iPriority);
void oledDrawTile(SSOLED *pOLED, const uint8_t *pTile, int x, int y,
                  int iRotation, int bInvert, int bRender);
void oledEllipse(SSOLED *pOLED, int iCenterX, int iCenterY, int32_t iRadiusX,
                 int32_t iRadiusY, uint8_t ucColor, uint8_t bFilled);
void oledRectangle(SSOLED *pOLED, int x1, int y1, int x2, int y2,
                   uint8_t ucColor, uint8_t bFilled);
#if defined(_LINUX_) && defined(__cplusplus)
}
#endif
#endif
