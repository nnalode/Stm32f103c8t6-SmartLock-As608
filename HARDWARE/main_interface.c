/**
 ******************************************************************************
 * @file    main_interface.c
 * @brief   主界面显示、开锁/关锁逻辑、密码验证、指纹/刷卡处理
 * @author  yl+ds
 ******************************************************************************
 */
#include "main_interface.h"
#include "main.h"
#include "oled.h"
#include "usart.h"
#include "Relay.h"
#include "Beep.h"
#include "key.h"
#include "ds1302.h"
#include "at24c02.h"
#include "as608.h"
#include "rc522.h"
#include "menu_system.h"
#include "system_state.h"
#include "onenet.h"
#include "soft_timer.h"
#include <stdio.h>
#include <string.h>

extern uint8_t lock_flag;
extern uint8_t admin_logged_in;
extern SystemState sys_state;
extern uint32_t menu_idle_timeout;
extern uint8_t add_flag;

static void ClearLine(uint8_t y);

/**
 * @brief  清除 OLED 指定行
 * @param  y: 像素 y 坐标（0~63）
 */
static void ClearLine(uint8_t y) {
    uint8_t page = y / 8;
    uint16_t start = page * 128;
    for (uint16_t col = 0; col < 128; col++) framebuffer[start + col] = 0x00;
}

/**
 * @brief  密码输入函数（6 位密码，支持回删和确认）
 * @param  out_buf: 输出密码缓冲区（至少 7 字节）
 * @param  len: 密码长度
 * @return 0-输入完成，1-取消或超时
 */
uint8_t InputPassword(char *out_buf, uint8_t len) {
    uint8_t pos = 0;
    uint32_t timeout = HAL_GetTick() + 30000;
    char display_buf[20] = {0};
    uint16_t last_key = 0;
    uint8_t key_released = 1;
    ClearLine(24);
    OLED_Update();
    while (HAL_GetTick() < timeout) {
        uint16_t key = Key_scan();
        if (key != 0) {
            if (key == last_key && !key_released) { HAL_Delay(20); continue; }
            last_key = key;
            key_released = 0;
            if ((key >= 1 && key <= 9) || key == 10) {
                if (pos < len) {
                    char digit = (key == 10) ? '0' : ('0' + key);
                    out_buf[pos++] = digit;
                    ClearLine(24);
                    memset(display_buf, '*', pos);
                    display_buf[pos] = '\0';
                    OLED_ShowStr(0, 24, display_buf, 1);
                    OLED_Update();
                }
            } else if (key == 13) { // 回删
                if (pos > 0) {
                    pos--;
                    out_buf[pos] = 0;
                    ClearLine(24);
                    if (pos == 0) OLED_ShowStr(0, 24, "_", 1);
                    else {
                        memset(display_buf, '*', pos);
                        display_buf[pos] = '\0';
                        OLED_ShowStr(0, 24, display_buf, 1);
                    }
                    OLED_Update();
                }
            } else if (key == 14) { // 确认
                if (pos == len) { out_buf[pos] = '\0'; return 0; }
                else {
                    OLED_ShowStr(0, 32, "Need 6 digits!", 1);
                    OLED_Update();
                    HAL_Delay(1000);
                    ClearLine(32);
                    OLED_Update();
                }
            } else if (key == 15) return 1; // 取消
            while (Key_scan() != 0) HAL_Delay(10);
            key_released = 1;
            last_key = 0;
        }
        HAL_Delay(20);
    }
    OLED_ShowStr(0, 40, "Timeout!", 1);
    OLED_Update();
    HAL_Delay(1000);
    return 1;
}

/**
 * @brief  管理员密码验证
 * @return 1-验证通过，0-取消或失败
 */
uint8_t Admin_VerifyPassword(void) {
    char stored_pwd[7] = {0};
    char input_pwd[7] = {0};
    AT24C02_ReadBytes(ADMIN_PWD_ADDR, (uint8_t*)stored_pwd, 6);
    if (stored_pwd[0] == 0xFF || stored_pwd[0] == 0x00) {
        strcpy(stored_pwd, "123456");
        AT24C02_WriteBytes(ADMIN_PWD_ADDR, (uint8_t*)stored_pwd, 6);
    }
    while (1) {
        OLED_CLS();
        OLED_ShowStr(0, 8, "Admin Login", 2);
        OLED_ShowStr(0, 16, "Enter 6-digit pwd", 1);
        OLED_ShowStr(0, 56, "Back to cancel", 1);
        OLED_Update();
        if (InputPassword(input_pwd, 6) == 0) {
            if (memcmp(input_pwd, stored_pwd, 6) == 0) return 1;
            else {
                OLED_CLS(); OLED_ShowStr(0, 16, "Wrong Password!", 2); OLED_Update();
                HAL_Delay(1500);
            }
        } else {
            OLED_CLS(); OLED_ShowStr(0, 16, "Cancelled", 1); OLED_Update();
            HAL_Delay(1000);
            return 0;
        }
    }
}

/**
 * @brief  用户密码验证（普通开门）
 * @return 1-验证通过，0-取消或失败
 */
uint8_t User_VerifyPassword(void) {
    char stored_pwd[7] = {0};
    char input_pwd[7] = {0};
    static uint8_t err_cnt = 0;
    AT24C02_ReadBytes(USER_PWD_ADDR, (uint8_t*)stored_pwd, 6);
    if (stored_pwd[0] == 0xFF || stored_pwd[0] == 0x00) {
        strcpy(stored_pwd, "123456");
        AT24C02_WriteBytes(USER_PWD_ADDR, (uint8_t*)stored_pwd, 6);
    }
    OLED_CLS();
    OLED_ShowStr(0, 8, "User Login", 2);
    OLED_ShowStr(0, 16, "Enter 6-digit pwd", 1);
    OLED_ShowStr(0, 56, "Back to cancel", 1);
    OLED_Update();
    if (InputPassword(input_pwd, 6) == 0) {
        if (memcmp(input_pwd, stored_pwd, 6) == 0) {
            err_cnt = 0;
            Log_UnlockEvent(UNLOCK_WAY_PASSWORD, 0, 0, 0, 0);
            return 1;
        } else {
            err_cnt++;
            OLED_CLS(); OLED_ShowStr(0, 16, "Wrong Password!", 2); OLED_Update();
            HAL_Delay(1500);
            if (err_cnt >= 5) {
                OLED_ShowStr(0, 32, "Too many errors", 1); OLED_Update();
                HAL_Delay(2000);
                err_cnt = 0;
            }
            MainInterface_Display();
            return 0;
        }
    } else {
        OLED_CLS(); OLED_ShowStr(0, 16, "Cancelled", 1); OLED_Update();
        HAL_Delay(1000);
        MainInterface_Display();
        return 0;
    }
}

/**
 * @brief  开锁动作
 */
void Func_Unlock(void) {
    printf("Unlock\r\n");
    Relay_Pulse_Start(500);
    Beep_Alarm_Start(2);
    lock_flag = 1;
    extern SoftTimer auto_lock_timer;
    extern uint8_t auto_lock_pending;
    if (auto_lock_pending) SoftTimer_Stop(&auto_lock_timer);
    SoftTimer_Start(&auto_lock_timer, 5000);
    auto_lock_pending = 1;
    OLED_CLS();
    OLED_ShowStr(0, 16, "Door Opened!", 2);
    OLED_Update();
    HAL_Delay(1000);
    MainInterface_Display();
}

/**
 * @brief  关锁动作
 */
void Func_Lock(void) {
    printf("Lock\r\n");
    Relay_Off();
    lock_flag = 0;
    Beep_Alarm_Start(1);
    extern SoftTimer auto_lock_timer;
    extern uint8_t auto_lock_pending;
    if (auto_lock_pending) {
        SoftTimer_Stop(&auto_lock_timer);
        auto_lock_pending = 0;
    }
    OLED_CLS();
    OLED_ShowStr(0, 16, "Door Locked!", 2);
    OLED_Update();
    HAL_Delay(1000);
    MainInterface_Display();
}

/**
 * @brief  显示主界面（时间、门状态、提示）
 */
void MainInterface_Display(void) {
    DS1302_GetTime();
    char buf[24];
    OLED_CLS();
    sprintf(buf, "20%02d-%02d-%02d %02d:%02d:%02d",
            Time[YEAR], Time[MONTH], Time[DATE],
            Time[HOUR], Time[MINUTE], Time[SECOND]);
    OLED_ShowStr(0, 0, buf, 1);
    OLED_ShowStr(0, 16, "Door:", 1);
    OLED_ShowStr(48, 16, lock_flag ? "Open " : "Close", 1);
    OLED_ShowStr(0, 32, "Place finger", 1);
    OLED_ShowStr(0, 40, "or RFID card", 1);
    if (admin_logged_in)
        OLED_ShowStr(0, 56, "Admin Mode", 1);
    else
        OLED_ShowStr(0, 56, "Long press #", 1);
    OLED_Update();
}

/**
 * @brief  指纹验证（阻塞式）
 */
void Verify_Fingerprint(void) {
    uint8_t res;
    SearchResult sr;
    OLED_CLS();
    OLED_ShowStr(0, 24, "Place finger...", 1);
    OLED_Update();
    res = PS_GetImage_Block(2000);
    if (res != 0x00) {
        OLED_ShowStr(0, 32, "No finger", 1);
        OLED_Update();
        HAL_Delay(800);
        if (sys_state == STATE_MAIN_IDLE) MainInterface_Display();
        else if (sys_state == STATE_MENU) Menu_Display();
        return;
    }
    res = PS_GenChar(CharBuffer1);
    if (res != 0x00) {
        OLED_ShowStr(0, 32, "GenChar fail", 1);
        OLED_Update();
        HAL_Delay(800);
        if (sys_state == STATE_MAIN_IDLE) MainInterface_Display();
        else Menu_Display();
        return;
    }
    res = PS_HighSpeedSearch(CharBuffer1, 0, 99, &sr);
    if (res == 0x00) {
        OLED_CLS();
        OLED_ShowStr(0, 32, "Verify OK!", 2);
        OLED_Update();
        Func_Unlock();
        Log_UnlockEvent(UNLOCK_WAY_FINGER, (uint8_t)sr.pageID, (uint8_t)(sr.pageID >> 8), 0, 0);
        HAL_Delay(1500);
    } else {
        OLED_CLS();
        OLED_ShowStr(0, 32, "Verify Failed", 1);
        OLED_Update();
        HAL_Delay(1000);
    }
    if (sys_state == STATE_MAIN_IDLE) MainInterface_Display();
    else if (sys_state == STATE_MENU) Menu_Display();
}

/**
 * @brief  刷卡检测（在主循环中轮询）
 */
void CheckRfidCard(void) {
    uint8_t tag_type[2];
    uint8_t uid[4];
    static uint32_t last_check = 0;
    if (sys_state == STATE_MENU) return;
    if (HAL_GetTick() - last_check < 50) return;
    last_check = HAL_GetTick();
    if (PcdRequest(PICC_REQALL, tag_type) == MI_OK) {
        if (PcdAnticoll(uid) == MI_OK) {
            if (IC_CheckCard(uid)) {
                printf("Card authorized\r\n");
                Log_UnlockEvent(UNLOCK_WAY_ICCARD, uid[0], uid[1], uid[2], uid[3]);
                Func_Unlock();
            } else {
                printf("Unauthorized card\r\n");
                Beep_Alarm_Start(2);
                OLED_CLS();
                OLED_ShowStr(0, 16, "Unauthorized!", 2);
                OLED_Update();
                HAL_Delay(800);
                if (sys_state == STATE_MAIN_IDLE) MainInterface_Display();
            }
            PcdHalt();
        }
    }
}