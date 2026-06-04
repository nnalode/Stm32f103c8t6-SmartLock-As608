#ifndef _KEY_H_
#define _KEY_H_

#include "stm32f1xx_hal.h"

void Key_GPIO_Init(void);
uint16_t Key_scan(void);
uint16_t Key_LongPressed(void);

#endif