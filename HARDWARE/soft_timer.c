#include "soft_timer.h"
#include "main.h"

void SoftTimer_Start(SoftTimer *timer, uint32_t duration_ms) {
    timer->start_tick = HAL_GetTick();
    timer->duration_ms = duration_ms;
    timer->active = true;
}

bool SoftTimer_IsExpired(SoftTimer *timer) {
    if (!timer->active) return false;
    if ((HAL_GetTick() - timer->start_tick) >= timer->duration_ms) {
        timer->active = false;
        return true;
    }
    return false;
}

void SoftTimer_Stop(SoftTimer *timer) {
    timer->active = false;
}