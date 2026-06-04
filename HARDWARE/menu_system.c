#include "menu_system.h"
#include "oled.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include "Relay.h"
#include "Beep.h"
#include "as608.h"
#include "ds1302.h"
#include "at24c02.h"
#include "main.h"  
#include "system_state.h"
#include "main_interface.h"
#include "soft_timer.h"
#include "rc522.h"
#include "key.h"   
#include "onenet.h"

extern uint32_t menu_idle_timeout;
static SoftTimer menu_return_timer;
static uint8_t menu_return_pending = 0;

static LogEntry log_entries[MAX_LOG_COUNT];
static uint8_t log_total = 0;
static uint8_t log_sel = 0;
static uint8_t log_view_active = 0;
extern uint8_t lock_flag;
extern uint8_t Store_Id[3];
extern uint16_t ID_NUM_store;
extern uint8_t add_flag;
extern uint8_t Time[TIME_SUM];

Menu *g_current_menu = NULL;

static Menu main_menu;
static Menu fingerprint_menu;
static Menu iccard_menu;

static MenuItem main_items[] = {
    {"Unlock",          Func_Unlock,            NULL},
    {"Lock",            Func_Lock,              NULL},
    {"Fingerprint",     NULL,                   &fingerprint_menu},
    {"IC Card",         NULL,                   &iccard_menu},
    {"Change User Pwd", Func_ChangeUserPwdOnly, NULL},
    {"Change Admin Pwd",Func_ChangeAdminPwd,    NULL},
    {"View Logs",       Func_ViewLogs,          NULL},
    {"Exit Admin",      Func_AdminLogout,       NULL},
};

static MenuItem fingerprint_items[] = {
    {"Add Fingerprint",   Func_AddFingerprint,   NULL},
    {"Delete One FP",     Func_DelOneFingerprint, NULL},
    {"Delete All FP",     Func_DelFingerprint,   NULL},
};

static MenuItem iccard_items[] = {
    {"Add IC Card",     Func_AddICCard,     NULL},
    {"Delete IC Card",  Func_DelICCard,     NULL},
};

static Menu main_menu = {
    .title = "Main Menu",
    .items = main_items,
    .item_count = sizeof(main_items) / sizeof(MenuItem),
    .current_sel = 0,
    .parent = NULL
};

static Menu fingerprint_menu = {
    .title = "Fingerprint",
    .items = fingerprint_items,
    .item_count = sizeof(fingerprint_items) / sizeof(MenuItem),
    .current_sel = 0,
    .parent = &main_menu
};

static Menu iccard_menu = {
    .title = "IC Card Mgmt",
    .items = iccard_items,
    .item_count = sizeof(iccard_items) / sizeof(MenuItem),
    .current_sel = 0,
    .parent = &main_menu
};

/**
 * @brief  菜单初始化，设置当前菜单为主菜单并显示
 * @author yl+ds
 */
void Menu_Init(void) {
    g_current_menu = &main_menu;
    Menu_Display();
    Menu_DisplayOnSerial();
}

/**
 * @brief  处理菜单操作（上下、进入、返回）
 * @param  op: 操作码
 * @author yl+ds
 */
void Menu_ProcessOp(MenuOp op) {
    switch (op) {
        case MENU_OP_UP:
            if (g_current_menu->current_sel > 0)
                g_current_menu->current_sel--;
            else
                g_current_menu->current_sel = g_current_menu->item_count - 1;
            Menu_Display();
            Menu_DisplayOnSerial();
            break;
        case MENU_OP_DOWN:
            if (g_current_menu->current_sel < g_current_menu->item_count - 1)
                g_current_menu->current_sel++;
            else
                g_current_menu->current_sel = 0;
            Menu_Display();
            Menu_DisplayOnSerial();
            break;
        case MENU_OP_ENTER: {
            MenuItem *item = &g_current_menu->items[g_current_menu->current_sel];
            if (item->func != NULL) {
                item->func();
                Menu_Display();
                Menu_DisplayOnSerial();
            } else if (item->sub_menu != NULL) {
                g_current_menu = item->sub_menu;
                g_current_menu->current_sel = 0;
                Menu_Display();
                Menu_DisplayOnSerial();
            }
            break;
        }
        case MENU_OP_BACK:
            if (g_current_menu->parent != NULL) {
                g_current_menu = g_current_menu->parent;
                Menu_Display();
                Menu_DisplayOnSerial();
            } else {
                if (g_current_menu == &main_menu) {
                    Func_AdminLogout();
                }
            }
            break;
    }
}

/**
 * @brief  获取日志类型字符串
 * @param  op_type: 操作类型
 * @param  info0: 开锁方式子类型
 * @return 字符串
 * @author yl+ds
 */
static const char* GetLogTypeString(uint8_t op_type, uint8_t info0) {
    switch(op_type) {
        case LOG_TYPE_UNLOCK:
            switch(info0) {
                case UNLOCK_WAY_FINGER:   return "F-Open";
                case UNLOCK_WAY_PASSWORD: return "P-Open";
                case UNLOCK_WAY_ICCARD:   return "C-Open";
                case UNLOCK_WAY_REMOTE:   return "R-Open";
                default:                  return "Open";
            }
        case LOG_TYPE_CHANGE_USER_PWD:  return "ChgUsrPwd";
        case LOG_TYPE_CHANGE_ADMIN_PWD: return "ChgAdmPwd";
        case LOG_TYPE_ADD_FINGER:       return "AddFp";
        case LOG_TYPE_DEL_FINGER:       return "DelFp";
        case LOG_TYPE_ADD_ICCARD:       return "AddCard";
        case LOG_TYPE_DEL_ICCARD:       return "DelCard";
        default:                        return "Unknown";
    }
}

/**
 * @brief  查看日志（管理员）
 * @author yl+ds
 */
void Func_ViewLogs(void) {
    if (!admin_logged_in) {
        OLED_CLS(); OLED_ShowStr(0,16,"Admin only!",1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    
    log_total = 0;
    uint8_t start_idx = AT24C02_ReadByte(LOG_START_ADDR);
    uint8_t count = AT24C02_ReadByte(LOG_START_ADDR + 1);
    if (count > MAX_LOG_COUNT) count = MAX_LOG_COUNT;
    
    uint8_t idx;
    for (uint8_t i = 0; i < count; i++) {
        if (count == MAX_LOG_COUNT)
            idx = (start_idx + i) % MAX_LOG_COUNT;
        else
            idx = i;
        uint16_t addr = LOG_START_ADDR + 2 + idx * LOG_ENTRY_SIZE;
        log_entries[i].year   = AT24C02_ReadByte(addr);
        log_entries[i].month  = AT24C02_ReadByte(addr + 1);
        log_entries[i].day    = AT24C02_ReadByte(addr + 2);
        log_entries[i].hour   = AT24C02_ReadByte(addr + 3);
        log_entries[i].minute = AT24C02_ReadByte(addr + 4);
        log_entries[i].second = AT24C02_ReadByte(addr + 5);
        log_entries[i].op_type= AT24C02_ReadByte(addr + 6);
        for (int j=0; j<4; j++)
            log_entries[i].info[j] = AT24C02_ReadByte(addr + 7 + j);
    }
    log_total = count;
    log_sel = 0;
    log_view_active = 1;
    
    uint16_t key;
    while (log_view_active) {
        OLED_CLS();
        OLED_ShowStr(0, 0, "Operation Logs", 1);
        OLED_ShowStr(0, 8, "----------------", 1);
        
        uint8_t start = (log_sel / 4) * 4;
        for (uint8_t i = 0; i < 4 && (start + i) < log_total; i++) {
            uint8_t idx_log = start + i;
            LogEntry *log = &log_entries[idx_log];
            char buf[24];
            snprintf(buf, sizeof(buf), "%02d-%02d %02d:%02d %s",
                     log->month, log->day, log->hour, log->minute,
                     GetLogTypeString(log->op_type, log->info[0]));
            if (strlen(buf) > 20) buf[20] = '\0';
            
            if (idx_log == log_sel) {
                OLED_ShowChar(0, 16 + i*8, '>', 1);
                OLED_ShowStr(8, 16 + i*8, buf, 1);
            } else {
                OLED_ShowStr(8, 16 + i*8, buf, 1);
            }
        }
        OLED_ShowStr(0, 56, "UP/DOWN: sel, BACK: exit", 1);
        OLED_Update();
        
        key = Key_scan();
        if (key == 11 && log_sel > 0) log_sel--;
        else if (key == 12 && log_sel < log_total-1) log_sel++;
        else if (key == 15) log_view_active = 0;
        HAL_Delay(100);
    }
    
    Menu_Display();
}

/**
 * @brief  显示当前菜单到 OLED
 * @author yl+ds
 */
void Menu_Display(void) {
    OLED_CLS();
    OLED_ShowStr(0, 0, g_current_menu->title, 1);
    OLED_ShowStr(0, 8, "----------------", 1);
    uint8_t start = 0;
    uint8_t max_lines = 4;
    if (g_current_menu->item_count > max_lines) {
        if (g_current_menu->current_sel >= max_lines) {
            start = g_current_menu->current_sel - max_lines + 1;
        }
    }
    for (uint8_t i = 0; i < max_lines && (start + i) < g_current_menu->item_count; i++) {
        uint8_t idx = start + i;
        uint8_t y_pixel = 16 + i * 8;
        if (idx == g_current_menu->current_sel) {
            OLED_ShowChar(0, y_pixel, '>', 2);
        } else {
            OLED_ShowChar(0, y_pixel, ' ', 2);
        }
        OLED_ShowStr(16, y_pixel, g_current_menu->items[idx].name, 1);
    }
    OLED_Update();
}

/**
 * @brief  通过串口打印菜单
 * @author yl+ds
 */
void Menu_DisplayOnSerial(void) {
    printf("\r\n=== %s ===\r\n", g_current_menu->title);
    for (uint8_t i = 0; i < g_current_menu->item_count; i++) {
        if (i == g_current_menu->current_sel)
            printf("> %d. %s\r\n", i + 1, g_current_menu->items[i].name);
        else
            printf("  %d. %s\r\n", i + 1, g_current_menu->items[i].name);
    }
    printf("========================\r\n");
}

/**
 * @brief  启动菜单返回延迟（如操作完成后自动返回）
 * @param  ms: 延迟毫秒
 * @author yl+ds
 */
void Menu_ReturnAfterDelay(uint32_t ms) {
    SoftTimer_Start(&menu_return_timer, ms);
    menu_return_pending = 1;
}

/**
 * @brief  检查并处理菜单延迟返回
 * @author yl+ds
 */
void Menu_Update(void) {
    if (menu_return_pending && SoftTimer_IsExpired(&menu_return_timer)) {
        menu_return_pending = 0;
        Menu_Display();
        Menu_DisplayOnSerial();
        printf("Menu timeout, resumed\r\n");
    }
}

/**
 * @brief  查询是否正在延迟返回
 * @return 1:是  0:否
 * @author yl+ds
 */
uint8_t Menu_IsReturnPending(void) {
    return menu_return_pending;
}

/**
 * @brief  修改管理员密码
 * @author yl+ds
 */
void Func_ChangeAdminPwd(void) {
    if (!admin_logged_in) {
        OLED_CLS(); OLED_ShowStr(0,16,"Admin only!",1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    OLED_CLS();
    OLED_ShowStr(0, 16, "Old Admin Pwd:", 1);
    OLED_Update();
    char old_pwd[7] = {0};
    if (InputPassword(old_pwd, 6) != 0) {
        OLED_CLS(); OLED_ShowStr(0, 16, "Cancelled", 1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    char stored_pwd[7] = {0};
    AT24C02_ReadBytes(ADMIN_PWD_ADDR, (uint8_t*)stored_pwd, 6);
    if (memcmp(old_pwd, stored_pwd, 6) != 0) {
        OLED_CLS(); OLED_ShowStr(0, 16, "Wrong Pwd!", 1); OLED_Update();
        HAL_Delay(1500);
        Menu_Display();
        return;
    }
    char new_pwd[7] = {0}, confirm_pwd[7] = {0};
    OLED_CLS(); OLED_ShowStr(0, 16, "New Admin Pwd:", 1); OLED_Update();
    if (InputPassword(new_pwd, 6) != 0) {
        OLED_CLS(); OLED_ShowStr(0, 16, "Cancelled", 1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    OLED_CLS(); OLED_ShowStr(0, 16, "Confirm New:", 1); OLED_Update();
    if (InputPassword(confirm_pwd, 6) != 0) {
        OLED_CLS(); OLED_ShowStr(0, 16, "Cancelled", 1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    if (strncmp(new_pwd, confirm_pwd, 6) == 0) {
        if (AT24C02_WriteBytes(ADMIN_PWD_ADDR, (uint8_t*)new_pwd, 6) == 0) {
            OLED_CLS(); OLED_ShowStr(0, 16, "Admin Pwd Changed", 1); OLED_Update();
            Log_RecordAndUpload(LOG_TYPE_CHANGE_ADMIN_PWD, NULL, 0);
        } else {
            OLED_CLS(); OLED_ShowStr(0, 16, "Write Failed!", 1); OLED_Update();
        }
    } else {
        OLED_CLS(); OLED_ShowStr(0, 16, "Not Match!", 1); OLED_Update();
    }
    HAL_Delay(1500);
    Menu_Display();
}

/**
 * @brief  修改用户密码（需管理员验证）
 * @author yl+ds
 */
void Func_ChangeUserPwdOnly(void) {
    OLED_CLS();
    OLED_ShowStr(0, 16, "Admin Verify", 2);
    OLED_Update();
    HAL_Delay(1000);
    if (Admin_VerifyPassword() == 0) {
        OLED_CLS();
        OLED_ShowStr(0, 16, "Verify Failed!", 2);
        OLED_Update();
        HAL_Delay(1500);
        Menu_Display();
        return;
    }
    char new_pwd[7] = {0};
    char confirm_pwd[7] = {0};
    OLED_CLS();
    OLED_ShowStr(0, 16, "New User Pwd:", 1);
    OLED_Update();
    if (InputPassword(new_pwd, 6) != 0) {
        OLED_CLS(); OLED_ShowStr(0, 16, "Cancelled", 1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    OLED_CLS();
    OLED_ShowStr(0, 16, "Confirm Pwd:", 1);
    OLED_Update();
    if (InputPassword(confirm_pwd, 6) != 0) {
        OLED_CLS(); OLED_ShowStr(0, 16, "Cancelled", 1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    if (strncmp(new_pwd, confirm_pwd, 6) == 0) {
        if (AT24C02_WriteBytes(USER_PWD_ADDR, (uint8_t*)new_pwd, 6) == 0) {
            char verify[7] = {0};
            AT24C02_ReadBytes(USER_PWD_ADDR, (uint8_t*)verify, 6);
            if (memcmp(new_pwd, verify, 6) == 0) {
                Log_RecordAndUpload(LOG_TYPE_CHANGE_USER_PWD, NULL, 0);
                OLED_CLS();
                OLED_ShowStr(0, 16, "User Pwd Changed", 1);
                OLED_Update();
                HAL_Delay(1500);
                Menu_Display();
            } else {
                OLED_CLS(); OLED_ShowStr(0, 16, "Verify Failed!", 1); OLED_Update();
                HAL_Delay(1500);
                Menu_Display();
            }
        } else {
            OLED_CLS(); OLED_ShowStr(0, 16, "Write Failed!", 1); OLED_Update();
            HAL_Delay(1500);
            Menu_Display();
        }
    } else {
        OLED_CLS(); OLED_ShowStr(0, 16, "Not match", 1); OLED_Update();
        HAL_Delay(1500);
        Menu_Display();
    }
}

/**
 * @brief  添加指纹（管理員）
 * @author yl+ds
 */
void Func_AddFingerprint(void) {
    if (!admin_logged_in) {
        OLED_CLS(); OLED_ShowStr(0,16,"Admin only!",1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    Add_FR_Blocking();
}

/**
 * @brief  删除单个指纹
 * @author yl+ds
 */
void Func_DelOneFingerprint(void) {
    if (!admin_logged_in) { return; }

    LoadFingerSlotUsage();

    uint8_t selected_id = SelectFingerToDelete();
    if (selected_id == 0xFF) {
        OLED_CLS();
        OLED_ShowStr(0, 16, "Deletion cancelled", 1);
        OLED_Update();
        HAL_Delay(1500);
        Menu_Display();
        return;
    }

    PS_Wakeup();
    HAL_Delay(100);

    uint8_t res = PS_DeleteTemplate(selected_id);
    if (res == 0x00) {
        if (selected_id >= 2 && selected_id <= 7) {
            finger_slot_usage &= ~(1 << (selected_id - 2));
            SaveFingerSlotUsage();
        }
        OLED_CLS();
        OLED_ShowStr(0, 16, "Finger deleted!", 2);
        char buf[20];
        sprintf(buf, "ID: %d", selected_id);
        OLED_ShowStr(0, 32, buf, 1);
        OLED_Update();
        HAL_Delay(2000);
    } else {
        OLED_CLS();
        OLED_ShowStr(0, 16, "Delete failed", 1);
        OLED_ShowStr(0, 24, EnsureMessage(res), 1);
        OLED_Update();
        HAL_Delay(1500);
    }
    Menu_Display();
}

/**
 * @brief  删除所有指纹
 * @author yl+ds
 */
void Func_DelFingerprint(void) {
    if (!admin_logged_in) { return; }

    LoadFingerSlotUsage();
    if (finger_slot_usage == 0) {
        OLED_CLS();
        OLED_ShowStr(0, 16, "No fingerprints", 1);
        OLED_Update();
        HAL_Delay(1500);
        Menu_Display();
        return;
    }

    OLED_CLS();
    OLED_ShowStr(0, 16, "Delete ALL?", 2);
    OLED_ShowStr(0, 32, "Press ENTER to confirm", 1);
    OLED_Update();
    uint16_t key = 0;
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 5000) {
        key = Key_scan();
        if (key == 14) break;
        if (key == 15) {
            Menu_Display();
            return;
        }
        HAL_Delay(50);
    }
    if (key != 14) {
        OLED_ShowStr(0, 48, "Cancelled", 1);
        OLED_Update();
        HAL_Delay(1000);
        Menu_Display();
        return;
    }

    PS_Wakeup();
    uint8_t res = PS_Empty();
    if (res == 0x00) {
        finger_slot_usage = 0;
        SaveFingerSlotUsage();
        OLED_CLS();
        OLED_ShowStr(0, 16, "All deleted!", 2);
        OLED_Update();
        HAL_Delay(2000);
    } else {
        OLED_CLS();
        OLED_ShowStr(0, 16, "Delete failed!", 1);
        OLED_ShowStr(0, 24, EnsureMessage(res), 1);
        OLED_Update();
        HAL_Delay(1500);
    }
    Menu_Display();
}

/**
 * @brief  管理员注销
 * @author yl+ds
 */
void Func_AdminLogout(void) {
    admin_logged_in = 0;
    extern SystemState sys_state;
    sys_state = STATE_MAIN_IDLE;
    OLED_CLS();
    MainInterface_Display();
}

/**
 * @brief  显示系统信息（锁状态、指纹数量、时间）
 * @author yl+ds
 */
void Func_ShowSystemInfo(void) {
    OLED_CLS();
    OLED_ShowStr(0, 0, "System Info", 1);
    char buf[20];
    sprintf(buf, "Lock: %s", lock_flag ? "Open" : "Close");
    OLED_ShowStr(0, 16, buf, 1);
    sprintf(buf, "Finger: %d", ID_NUM_store);
    OLED_ShowStr(0, 24, buf, 1);
    DS1302_GetTime();
    sprintf(buf, "Time: %02d:%02d:%02d", Time[HOUR], Time[MINUTE], Time[SECOND]);
    OLED_ShowStr(0, 40, buf, 1);
    OLED_Update();
    Menu_ReturnAfterDelay(3000);
}

/**
 * @brief  等待刷卡（超时退出）
 * @param  uid: 输出 UID
 * @param  timeout_ms: 超时时间
 * @return 1:成功  0:超时
 * @author yl+ds
 */
static uint8_t WaitForCard(uint8_t *uid, uint32_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    uint8_t tag_type[2];
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (PcdRequest(PICC_REQALL, tag_type) == MI_OK) {
            for (int retry = 0; retry < 3; retry++) {
                if (PcdAnticoll(uid) == MI_OK) {
                    PcdHalt();
                    return 1;
                }
                HAL_Delay(5);
            }
        }
        HAL_Delay(100);
    }
    return 0;
}

/**
 * @brief  添加 IC 卡
 * @author yl+ds
 */
void Func_AddICCard(void) {
    if (!admin_logged_in) {
        OLED_CLS(); OLED_ShowStr(0,16,"Admin only!",1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    OLED_CLS();
    OLED_ShowStr(0, 16, "Place card near", 1);
    OLED_ShowStr(0, 24, "reader...", 1);
    OLED_Update();
    uint8_t uid[4];
    printf("Waiting for card...\r\n");
    if (WaitForCard(uid, 10000)) {
        printf("Card detected UID: %02X%02X%02X%02X\r\n", uid[0],uid[1],uid[2],uid[3]);
        uint8_t ret = IC_AddCard(uid);
        OLED_CLS();
        if (ret == 0) {
            Log_RecordAndUpload(LOG_TYPE_ADD_ICCARD, uid, 4);
            OLED_ShowStr(0, 16, "Card added!", 2);
            char buf[20];
            sprintf(buf, "UID: %02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
            OLED_ShowStr(0, 32, buf, 1);
        } else if (ret == 1) {
            OLED_ShowStr(0, 16, "Card already exists", 1);
        } else {
            OLED_ShowStr(0, 16, "Storage full", 1);
        }
        OLED_Update();
        HAL_Delay(2000);
    } else {
        OLED_CLS();
        OLED_ShowStr(0, 16, "Timeout, no card", 1);
        OLED_Update();
        HAL_Delay(2000);
    }
    Menu_ReturnAfterDelay(500);
}

/**
 * @brief  选择要删除的 IC 卡
 * @return 有效索引，0xFF 取消
 * @author yl+ds
 */
static uint8_t SelectCardToDelete(void) {
    uint8_t card_list[5][4];
    AT24C02_ReadBytes(IC_CARD_LIST_ADDR, (uint8_t*)card_list, 20);
    uint8_t valid_indices[5];
    uint8_t valid_count = 0;
    for (int i=0; i<5; i++) {
        int empty = 1;
        for (int j=0; j<4; j++) if (card_list[i][j]!=0xFF && card_list[i][j]!=0x00) { empty=0; break; }
        if (!empty) valid_indices[valid_count++] = i;
    }
    if (valid_count == 0) {
        OLED_CLS();
        OLED_ShowStr(0, 16, "No cards to delete", 1);
        OLED_Update();
        HAL_Delay(1500);
        return 0xFF;
    }
    uint8_t selection = 0;
    uint8_t key;
    while (1) {
        OLED_CLS();
        OLED_ShowStr(0, 0, "Select card to delete:", 1);
        for (uint8_t i=0; i<valid_count && i<4; i++) {
            uint8_t idx = valid_indices[i];
            char buf[30];
            sprintf(buf, "%d: %02X%02X%02X%02X", i+1,
                    card_list[idx][0], card_list[idx][1], card_list[idx][2], card_list[idx][3]);
            if (i == selection) {
                OLED_ShowChar(0, 16+i*8, '>', 1);
                OLED_ShowStr(8, 16+i*8, buf, 1);
            } else {
                OLED_ShowStr(8, 16+i*8, buf, 1);
            }
        }
        OLED_ShowStr(0, 56, "Enter: confirm, Back: cancel", 1);
        OLED_Update();
        key = Key_scan();
        if (key == 11) { if (selection>0) selection--; else selection = valid_count-1; HAL_Delay(200); }
        else if (key == 12) { if (selection<valid_count-1) selection++; else selection=0; HAL_Delay(200); }
        else if (key == 14) return valid_indices[selection];
        else if (key == 15) return 0xFF;
        HAL_Delay(50);
    }
}

/**
 * @brief  删除 IC 卡
 * @author yl+ds
 */
void Func_DelICCard(void) {
    if (!admin_logged_in) {
        OLED_CLS(); OLED_ShowStr(0,16,"Admin only!",1); OLED_Update();
        Menu_ReturnAfterDelay(1500);
        return;
    }
    uint8_t selected = SelectCardToDelete();
    if (selected == 0xFF) {
        OLED_CLS();
        OLED_ShowStr(0, 16, "Deletion cancelled", 1);
        OLED_Update();
        HAL_Delay(1500);
        Menu_ReturnAfterDelay(500);
        return;
    }
    uint8_t card_list[5][4];
    AT24C02_ReadBytes(IC_CARD_LIST_ADDR, (uint8_t*)card_list, 20);
    uint8_t *uid = card_list[selected];
    Log_RecordAndUpload(LOG_TYPE_DEL_ICCARD, uid, 4);
    memset(card_list[selected], 0xFF, 4);
    AT24C02_WriteBytes(IC_CARD_LIST_ADDR + selected*4, card_list[selected], 4);
    OLED_CLS();
    OLED_ShowStr(0, 16, "Card deleted!", 2);
    char buf[30];
    sprintf(buf, "UID: %02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
    OLED_ShowStr(0, 32, buf, 1);
    OLED_Update();
    HAL_Delay(2000);
    Menu_ReturnAfterDelay(500);
}

/**
 * @brief  重置菜单空闲超时
 * @author yl+ds
 */
void Menu_ResetIdleTimeout(void) {
    extern uint32_t menu_idle_timeout;
    menu_idle_timeout = HAL_GetTick() + 30000;
}

/**
 * @brief  选择要删除的指纹 ID
 * @return 指纹 ID，0xFF 取消
 * @author yl+ds
 */
static uint8_t SelectFingerToDelete(void) {
    uint8_t used_ids[6];
    uint8_t count = 0;
    for (int i = 0; i < 6; i++) {
        if (finger_slot_usage & (1 << i)) {
            used_ids[count++] = i + 2;
        }
    }
    if (count == 0) {
        OLED_CLS();
        OLED_ShowStr(0, 16, "No fingerprints", 1);
        OLED_Update();
        HAL_Delay(1500);
        return 0xFF;
    }

    uint8_t selection = 0;
    uint8_t key;
    while (1) {
        OLED_CLS();
        OLED_ShowStr(0, 0, "Select FP to delete:", 1);
        for (uint8_t i = 0; i < count && i < 4; i++) {
            char buf[20];
            sprintf(buf, "%d: Finger ID %d", i+1, used_ids[i]);
            if (i == selection) {
                OLED_ShowChar(0, 16 + i*8, '>', 1);
            } else {
                OLED_ShowChar(0, 16 + i*8, ' ', 1);
            }
            OLED_ShowStr(8, 16 + i*8, buf, 1);
        }
        OLED_ShowStr(0, 56, "Enter: del, Back: cancel", 1);
        OLED_Update();
        key = Key_scan();
        if (key == 11) { if (selection > 0) selection--; else selection = count - 1; HAL_Delay(200); }
        else if (key == 12) { if (selection < count - 1) selection++; else selection = 0; HAL_Delay(200); }
        else if (key == 14) return used_ids[selection];
        else if (key == 15) return 0xFF;
        HAL_Delay(50);
    }
}