#include "key.h"
#include "stm32f1xx_hal.h"
#include "oled.h"
#include "usart.h"
#include <stdio.h>

#define ROW1_PIN    GPIO_PIN_12
#define ROW2_PIN    GPIO_PIN_13
#define ROW3_PIN    GPIO_PIN_14
#define ROW4_PIN    GPIO_PIN_15
#define ROW_PORT    GPIOB

#define COL1_PORT   GPIOB
#define COL1_PIN    GPIO_PIN_8
#define COL2_PORT   GPIOB
#define COL2_PIN    GPIO_PIN_9
#define COL3_PORT   GPIOC
#define COL3_PIN    GPIO_PIN_14
#define COL4_PORT   GPIOC
#define COL4_PIN    GPIO_PIN_15

const uint16_t key_map[4][4] = {
    {4,  3, 2,  1,},
    {8,  7,  6,  5},
    {12, 11, 10, 9},
    {16, 15, 14, 13}
};

/**
 * @brief  初始化矩阵键盘 GPIO，释放 PC14/15 为普通 IO
 * @author yl+ds
 */
void Key_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSE_CONFIG(RCC_LSE_OFF);
    RCC->BDCR &= ~RCC_BDCR_LSEON;
    RCC->BDCR &= ~RCC_BDCR_LSEBYP;
    HAL_PWR_DisableBkUpAccess();

    GPIO_InitStruct.Pin = ROW1_PIN | ROW2_PIN | ROW3_PIN | ROW4_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(ROW_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(ROW_PORT, ROW1_PIN | ROW2_PIN | ROW3_PIN | ROW4_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = COL1_PIN | COL2_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(COL1_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = COL3_PIN | COL4_PIN;
    HAL_GPIO_Init(COL3_PORT, &GPIO_InitStruct);
    printf("BDCR = 0x%08X\r\n", RCC->BDCR);
}

/**
 * @brief  读取列状态（bit0:PB8, bit1:PB9, bit2:PC14, bit3:PC15，低电平有效）
 * @return 列状态掩码
 * @author yl+ds
 */
static uint8_t ReadColumns(void)
{
    uint8_t col_status = 0;
    if (HAL_GPIO_ReadPin(COL1_PORT, COL1_PIN) == GPIO_PIN_RESET)
        col_status |= 0x01;
    if (HAL_GPIO_ReadPin(COL2_PORT, COL2_PIN) == GPIO_PIN_RESET)
        col_status |= 0x02;
    if (HAL_GPIO_ReadPin(COL3_PORT, COL3_PIN) == GPIO_PIN_RESET)
        col_status |= 0x04;
    if (HAL_GPIO_ReadPin(COL4_PORT, COL4_PIN) == GPIO_PIN_RESET)
        col_status |= 0x08;
    return col_status;
}

/**
 * @brief  按键扫描（消抖 20ms）
 * @return 键值 1~16，0 表示无按键
 * @author yl+ds
 */
uint16_t Key_scan(void)
{
    uint8_t row;
    uint8_t col_status;
    uint16_t key_val = 0;

    for (row = 0; row < 4; row++)
    {
        switch (row)
        {
            case 0:
                HAL_GPIO_WritePin(ROW_PORT, ROW1_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(ROW_PORT, ROW2_PIN | ROW3_PIN | ROW4_PIN, GPIO_PIN_SET);
                break;
            case 1:
                HAL_GPIO_WritePin(ROW_PORT, ROW2_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(ROW_PORT, ROW1_PIN | ROW3_PIN | ROW4_PIN, GPIO_PIN_SET);
                break;
            case 2:
                HAL_GPIO_WritePin(ROW_PORT, ROW3_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(ROW_PORT, ROW1_PIN | ROW2_PIN | ROW4_PIN, GPIO_PIN_SET);
                break;
            case 3:
                HAL_GPIO_WritePin(ROW_PORT, ROW4_PIN, GPIO_PIN_RESET);
                HAL_GPIO_WritePin(ROW_PORT, ROW1_PIN | ROW2_PIN | ROW3_PIN, GPIO_PIN_SET);
                break;
        }
        HAL_Delay(1);
        col_status = ReadColumns();
        if (col_status != 0)
        {
            HAL_Delay(20);
            col_status = ReadColumns();
            if (col_status != 0)
            {
                if (col_status == 0x01)        key_val = key_map[row][0];
                else if (col_status == 0x02)   key_val = key_map[row][1];
                else if (col_status == 0x04)   key_val = key_map[row][2];
                else if (col_status == 0x08)   key_val = key_map[row][3];
                HAL_GPIO_WritePin(ROW_PORT, ROW1_PIN | ROW2_PIN | ROW3_PIN | ROW4_PIN, GPIO_PIN_RESET);
                return key_val;
            }
        }
    }
    HAL_GPIO_WritePin(ROW_PORT, ROW1_PIN | ROW2_PIN | ROW3_PIN | ROW4_PIN, GPIO_PIN_RESET);
    return 0;
}

/**
 * @brief  检测长按（≥800ms），消抖后触发一次
 * @return 键值，0 表示无长按
 * @author yl+ds
 */
uint16_t Key_LongPressed(void) {
    static uint16_t last_key = 0;
    static uint32_t press_start = 0;
    static uint8_t long_pressed_flag = 0;
    static uint8_t debounce_cnt = 0;

    uint16_t key = Key_scan();

    if (key != 0) {
        if (key == last_key) {
            if (debounce_cnt < 2) debounce_cnt++;
            if (debounce_cnt >= 2) {
                if (press_start == 0) {
                    press_start = HAL_GetTick();
                    long_pressed_flag = 0;
                } else if (!long_pressed_flag && (HAL_GetTick() - press_start) >= 800) {
                    long_pressed_flag = 1;
                    return key;
                }
            }
        } else {
            last_key = key;
            press_start = HAL_GetTick();
            long_pressed_flag = 0;
            debounce_cnt = 0;
        }
    } else {
        last_key = 0;
        press_start = 0;
        long_pressed_flag = 0;
        debounce_cnt = 0;
    }
    return 0;
}