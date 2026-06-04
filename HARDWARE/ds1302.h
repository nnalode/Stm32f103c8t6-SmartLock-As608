#ifndef __DS1302_H
#define __DS1302_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define DS1302_RST_PORT    GPIOB
#define DS1302_RST_PIN     GPIO_PIN_5
#define DS1302_DAT_PORT    GPIOB
#define DS1302_DAT_PIN     GPIO_PIN_4
#define DS1302_CLK_PORT    GPIOB
#define DS1302_CLK_PIN     GPIO_PIN_3

#define DS1302_RST_HIGH()  HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_SET)
#define DS1302_RST_LOW()   HAL_GPIO_WritePin(DS1302_RST_PORT, DS1302_RST_PIN, GPIO_PIN_RESET)
#define DS1302_DAT_HIGH()  HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, GPIO_PIN_SET)
#define DS1302_DAT_LOW()   HAL_GPIO_WritePin(DS1302_DAT_PORT, DS1302_DAT_PIN, GPIO_PIN_RESET)
#define DS1302_CLK_HIGH()  HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_SET)
#define DS1302_CLK_LOW()   HAL_GPIO_WritePin(DS1302_CLK_PORT, DS1302_CLK_PIN, GPIO_PIN_RESET)
#define DS1302_READ_DAT()  HAL_GPIO_ReadPin(DS1302_DAT_PORT, DS1302_DAT_PIN)

#define DS1302_SECOND     0x80
#define DS1302_MINUTE     0x82
#define DS1302_HOUR       0x84
#define DS1302_DAY        0x86
#define DS1302_MONTH      0x88
#define DS1302_WEEK       0x8A
#define DS1302_YEAR       0x8C
#define DS1302_WP         0x8E

typedef enum { SECOND, MINUTE, HOUR, DATE, MONTH, WEEK, YEAR, TIME_SUM } TIME_PARAM;

extern uint8_t Time[TIME_SUM];

void DS1302_Init(void);
void DS1302_WriteData(uint8_t cmd, uint8_t data);
uint8_t DS1302_ReadData(uint8_t cmd);
void DS1302_SetTime(void);
void DS1302_GetTime(void);
void DS1302_InitIfNeeded(uint8_t *default_time);

#endif