/**
  ******************************************************************************
  * @file    ds1302.c
  * @brief   DS1302 实时时钟驱动
  * @author  yl+ds (基于原代码整理)
  ******************************************************************************
  */
#include "ds1302.h"
#include "main.h"

static void delay_us(uint32_t us) {
    uint32_t i;
    for (i = 0; i < us * 8; i++) __NOP();
}

static void DS1302_SetDataOutput(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DS1302_DAT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DS1302_DAT_PORT, &GPIO_InitStruct);
}

static void DS1302_SetDataInput(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DS1302_DAT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DS1302_DAT_PORT, &GPIO_InitStruct);
}

static void Write_Ds1302_Byte(uint8_t temp) {
    uint8_t i;
    DS1302_SetDataOutput();
    for (i = 0; i < 8; i++) {
        if (temp & 0x01) DS1302_DAT_HIGH(); else DS1302_DAT_LOW();
        DS1302_CLK_LOW(); delay_us(1);
        temp >>= 1;
        DS1302_CLK_HIGH(); delay_us(1);
    }
}

static uint8_t Read_Ds1302_Byte(void) {
    uint8_t i, temp = 0;
    DS1302_SetDataInput();
    for (i = 0; i < 8; i++) {
        if (DS1302_READ_DAT()) temp |= 0x80;
        DS1302_CLK_LOW(); delay_us(1);
        temp >>= 1;
        DS1302_CLK_HIGH(); delay_us(1);
    }
    return temp;
}

void DS1302_WriteData(uint8_t cmd, uint8_t data) {
    DS1302_RST_LOW(); delay_us(1);
    DS1302_CLK_LOW(); delay_us(1);
    DS1302_RST_HIGH(); delay_us(1);
    Write_Ds1302_Byte(cmd);
    Write_Ds1302_Byte(data);
    DS1302_RST_LOW(); delay_us(1);
}

uint8_t DS1302_ReadData(uint8_t cmd) {
    uint8_t data;
    cmd |= 0x01;
    DS1302_RST_LOW(); delay_us(1);
    DS1302_CLK_LOW(); delay_us(1);
    DS1302_RST_HIGH(); delay_us(1);
    Write_Ds1302_Byte(cmd);
    data = Read_Ds1302_Byte();
    DS1302_RST_LOW(); delay_us(1);
    return data;
}

void DS1302_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitStruct.Pin = DS1302_RST_PIN | DS1302_CLK_PIN | DS1302_DAT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DS1302_RST_PORT, &GPIO_InitStruct);
    DS1302_RST_LOW();
    DS1302_CLK_LOW();
    DS1302_DAT_LOW();
}

void DS1302_SetTime(void) {
    DS1302_WriteData(DS1302_WP, 0x00);
    DS1302_WriteData(DS1302_SECOND, (Time[SECOND]/10)*16 + Time[SECOND]%10);
    DS1302_WriteData(DS1302_MINUTE, (Time[MINUTE]/10)*16 + Time[MINUTE]%10);
    DS1302_WriteData(DS1302_HOUR,   (Time[HOUR]/10)*16 + Time[HOUR]%10);
    DS1302_WriteData(DS1302_DAY,    (Time[DATE]/10)*16 + Time[DATE]%10);
    DS1302_WriteData(DS1302_MONTH,  (Time[MONTH]/10)*16 + Time[MONTH]%10);
    DS1302_WriteData(DS1302_WEEK,   Time[WEEK]);
    DS1302_WriteData(DS1302_YEAR,   (Time[YEAR]/10)*16 + Time[YEAR]%10);
    DS1302_WriteData(DS1302_WP, 0x80);
}

void DS1302_GetTime(void) {
    uint8_t temp;
    temp = DS1302_ReadData(DS1302_SECOND); Time[SECOND] = (temp/16)*10 + (temp%16);
    temp = DS1302_ReadData(DS1302_MINUTE); Time[MINUTE] = (temp/16)*10 + (temp%16);
    temp = DS1302_ReadData(DS1302_HOUR);   Time[HOUR]   = (temp/16)*10 + (temp%16);
    temp = DS1302_ReadData(DS1302_DAY);    Time[DATE]   = (temp/16)*10 + (temp%16);
    temp = DS1302_ReadData(DS1302_MONTH);  Time[MONTH]  = (temp/16)*10 + (temp%16);
    temp = DS1302_ReadData(DS1302_WEEK);   Time[WEEK]   = (temp/16)*10 + (temp%16);
    temp = DS1302_ReadData(DS1302_YEAR);   Time[YEAR]   = (temp/16)*10 + (temp%16);
}

void DS1302_InitIfNeeded(uint8_t *default_time) {
    DS1302_GetTime();
    if (Time[YEAR] < 20 || Time[YEAR] > 99 || Time[MONTH] == 0 || Time[MONTH] > 12 || Time[DATE] == 0) {
        for (int i = 0; i < TIME_SUM; i++) Time[i] = default_time[i];
        DS1302_SetTime();
    }
}

uint8_t Time[TIME_SUM] = {0, 30, 15, 5, 4, 7, 26};