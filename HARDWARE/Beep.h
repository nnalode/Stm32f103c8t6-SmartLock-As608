#ifndef __BEEP_H
#define __BEEP_H

#include "stm32f1xx_hal.h"

void Beep_Init(void);
void Beep_TurnOn(void);
void Beep_TurnOff(void);
void Beep_Alarm_Start(uint8_t count);
void Beep_Update(void);

#endif