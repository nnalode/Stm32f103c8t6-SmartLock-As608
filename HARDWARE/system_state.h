
#ifndef __SYSTEM_STATE_H
#define __SYSTEM_STATE_H
#include <stdint.h>
typedef enum {
    STATE_MAIN_IDLE,
    STATE_MENU
} SystemState;

extern SystemState sys_state;
extern uint8_t admin_logged_in;

#endif