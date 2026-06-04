#ifndef __MENU_SYSTEM_H
#define __MENU_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

extern uint8_t admin_logged_in;

typedef enum {
    MENU_OP_UP = 1,
    MENU_OP_DOWN,
    MENU_OP_ENTER,
    MENU_OP_BACK
} MenuOp;

typedef struct MenuItem {
    const char *name;
    void (*func)(void);
    struct Menu *sub_menu;
} MenuItem;

typedef struct Menu {
    const char *title;
    MenuItem *items;
    uint8_t item_count;
    uint8_t current_sel;
    struct Menu *parent;
} Menu;

extern Menu *g_current_menu;
static uint8_t SelectFingerToDelete(void);
void Menu_Init(void);
void Menu_ProcessOp(MenuOp op);
void Menu_Display(void);
void Menu_DisplayOnSerial(void);

void Func_Unlock(void);
void Func_Lock(void);
void Func_ChangeUserPwdOnly(void);
void Func_AddFingerprint(void);
void Func_DelOneFingerprint(void);
void Func_ChangeAdminPwd(void);
void Func_DelFingerprint(void);
void Func_ShowSystemInfo(void);
void Func_AdminLogout(void);
void Func_AddICCard(void);
void Func_DelICCard(void);
void Func_ViewLogs(void);
void Menu_Update(void);
void Menu_ReturnAfterDelay(uint32_t ms);
uint8_t Menu_IsReturnPending(void);
void Menu_ResetIdleTimeout(void);
#endif