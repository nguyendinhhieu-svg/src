#ifndef __SSD1306_H__
#define __SSD1306_H__

#include "stm32f1xx_hal.h"

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64

#define SSD1306_I2C_ADDR 0x78

void SSD1306_Init(void);

void SSD1306_Clear(void);
void SSD1306_Fill(uint8_t data);

void SSD1306_GotoXY(uint8_t x, uint8_t y);

void SSD1306_Puts(const char *str, uint8_t color);

void SSD1306_UpdateScreen(void);

void SSD1306_WriteCommand(uint8_t cmd);
void SSD1306_WriteData(uint8_t data);

#endif