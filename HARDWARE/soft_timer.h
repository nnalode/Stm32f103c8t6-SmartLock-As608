#ifndef __SOFT_TIMER_H
#define __SOFT_TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t start_tick;
    uint32_t duration_ms;
    bool active;
} SoftTimer;

void SoftTimer_Start(SoftTimer *timer, uint32_t duration_ms);
bool SoftTimer_IsExpired(SoftTimer *timer);
void SoftTimer_Stop(SoftTimer *timer);

#endif