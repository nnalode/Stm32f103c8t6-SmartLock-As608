/**
  * @file    beep.c
  * @brief   ·äÃùÆ÷¿ØÖÆ
  * @author  yl+ds
  */
#include "Beep.h"
#include "soft_timer.h"

#define BEEP_GPIO_PORT    GPIOA
#define BEEP_GPIO_PIN     GPIO_PIN_11

static SoftTimer beep_timer;
static uint8_t beep_count = 0;
static uint8_t beep_state = 0;

void Beep_Alarm_Start(uint8_t count) {
    if (count > 10) count = 10;
    beep_count = count;
    beep_state = 1;
    Beep_TurnOn();
    SoftTimer_Start(&beep_timer, 100);
}

void Beep_Update(void) {
    if (beep_state == 0) return;
    if (SoftTimer_IsExpired(&beep_timer)) {
        if (beep_state == 1) {
            Beep_TurnOff();
            beep_state = 2;
            SoftTimer_Start(&beep_timer, 100);
        } else if (beep_state == 2) {
            beep_count--;
            if (beep_count > 0) {
                beep_state = 1;
                Beep_TurnOn();
                SoftTimer_Start(&beep_timer, 100);
            } else beep_state = 0;
        }
    }
}

void Beep_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = BEEP_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BEEP_GPIO_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_SET);
}

void Beep_TurnOn(void)  { HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_RESET); }
void Beep_TurnOff(void) { HAL_GPIO_WritePin(BEEP_GPIO_PORT, BEEP_GPIO_PIN, GPIO_PIN_SET); }