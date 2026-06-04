/**
  * @file    relay.c
  * @brief   继电器控制（门锁输出）
  * @author  yl+ds
  */
#include "Relay.h"
#include "soft_timer.h"

static SoftTimer pulse_timer;
static uint8_t pulse_active = 0;

#define RELAY_PORT   GPIOA
#define RELAY_PIN    GPIO_PIN_12

void Relay_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = RELAY_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(RELAY_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET);
    pulse_active = 0;
}

void Relay_On(void)  { HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_SET); }
void Relay_Off(void) { HAL_GPIO_WritePin(RELAY_PORT, RELAY_PIN, GPIO_PIN_RESET); }

void Relay_Pulse_Start(uint16_t ms) {
    Relay_On();
    SoftTimer_Start(&pulse_timer, ms);
    pulse_active = 1;
}

void Relay_Update(void) {
    if (pulse_active && SoftTimer_IsExpired(&pulse_timer)) {
        Relay_Off();
        pulse_active = 0;
    }
}