#ifndef __RELAY_H
#define __RELAY_H

#include "stm32f1xx_hal.h"

void Relay_Init(void);
void Relay_On(void);
void Relay_Off(void);
void Relay_Pulse(uint16_t ms);   // Âö³å´¥·¢¿ªËø
void Relay_Pulse_Start(uint16_t ms);
void Relay_Update(void);
#endif