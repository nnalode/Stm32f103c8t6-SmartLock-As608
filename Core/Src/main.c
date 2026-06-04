/* USER CODE BEGIN Header */
#include "main.h"
#include "i2c.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "oled.h"
#include "as608.h"
#include "stdio.h"
#include "onenet.h"
#include "esp8266.h"
#include "key.h"
#include "rc522.h"
#include "delay.h"
#include "at24c02.h"
#include "ds1302.h"
#include "Relay.h"
#include "Beep.h"
#include "menu_system.h"
#include <string.h>
#include <stdbool.h>
#include "main_interface.h"
#include "system_state.h"
#include "soft_timer.h"
/* USER CODE END Includes */

uint8_t default_time[TIME_SUM] = {0, 30, 15, 5, 4, 7, 26};

/* Private variables */
extern unsigned char esp8266_buf[512];
extern short esp8266_cnt, esp8266_cntPre;
uint8_t rx_temp;
uint8_t rx_temp_uart3;

SystemState sys_state = STATE_MAIN_IDLE;
uint8_t admin_logged_in = 0;
uint32_t menu_idle_timeout = 0;
uint32_t last_main_refresh = 0;

// 指纹相关状态（原注释状态机已删除，只保留中断标志）
extern volatile uint8_t finger_touch_flag;   // 实际定义在 as608.c 中，此处声明为 extern? 确认 as608.c 定义了一次，这里不能重复定义。我们只做 extern 引用。
// 但原来 main.c 中重复定义了？会导致冲突。我们应该在 main.c 中删除这个定义，改为 extern 声明。
// 为保持兼容，main.c 中不定义 finger_touch_flag，在 as608.c 中定义。所以删除这一行。
// 下面已有的变量：
static uint8_t last_reported_lock_flag = 0xFF;
static uint32_t last_report_tick = 0;
uint8_t lock_flag = 0;
uint8_t g_user_pwd_err_cnt = 0;
uint16_t ID_NUM_store = 0;
uint8_t add_flag = 0;
SoftTimer auto_lock_timer;
uint8_t auto_lock_pending = 0;

extern void Add_FR(void);
extern void Store_Load(void);
extern void RC522_CheckVersion(void);
extern void test_fingerprint(void);
extern char PcdHalt(void);

void SystemClock_Config(void);
void Error_Handler(void);

/**
 * @brief  主函数
 */
int main(void) {
   HAL_Init();
    SystemClock_Config();

    /* ---------- 外设初始化 ---------- */
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_RTC_Init();
    MX_USART1_UART_Init();
    MX_USART3_UART_Init();
    MX_TIM3_Init();
    MX_SPI1_Init();
    MX_USART2_UART_Init();

    /* ---------- OLED 初始化 ---------- */
    OLED_Init();
    OLED_CLS();
    OLED_ShowStr(0, 0, "OLED OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- 继电器初始化 ---------- */
    Relay_Init();
    OLED_ShowStr(0, 16, "Relay OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- 蜂鸣器初始化 ---------- */
    Beep_Init();
    OLED_ShowStr(0, 24, "Beep OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- 按键初始化 ---------- */
    Key_GPIO_Init();
    OLED_ShowStr(0, 32, "Key OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- RC522 初始化 ---------- */
    OLED_ShowStr(0, 40, "RC522 Init...", 1);
    OLED_Update();
    RC522_Init();
    OLED_ShowStr(0, 40, "RC522 OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- 指纹模块初始化 ---------- */
    PS_StaGPIO_Init();
    uint32_t addr;
    if (PS_HandShake(&addr) == 0) {
        AS608Addr = addr;
        OLED_ShowStr(0, 48, "AS608 OK", 1);
    } else {
        OLED_ShowStr(0, 48, "AS608 Fail", 1);
    }
    OLED_Update();
    HAL_Delay(300);

    /* ---------- DS1302 时钟初始化 ---------- */
    DS1302_Init();
    DS1302_InitIfNeeded(default_time);
    OLED_CLS();
    OLED_ShowStr(0, 0, "RTC OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- EEPROM 加载指纹槽位 ---------- */
    LoadFingerSlotUsage();
    OLED_ShowStr(0, 8, "EEPROM OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- 日志系统初始化 ---------- */
    Log_Init();
    OLED_ShowStr(0, 16, "Log OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- 串口接收中断启动 ---------- */
    HAL_UART_Receive_IT(&huart2, &rx_temp, 1);
    HAL_UART_Receive_IT(&huart3, &rx_temp_uart3, 1);
    OLED_ShowStr(0, 24, "UART IT OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- WiFi 初始化 ---------- */
    OLED_ShowStr(0, 32, "WiFi Init...", 1);
    OLED_Update();
    ESP8266_Init();
    OLED_ShowStr(0, 32, "WiFi OK", 1);
    OLED_Update();
    HAL_Delay(300);

    /* ---------- MQTT 连接 OneNET ---------- */
    OLED_ShowStr(0, 40, "MQTT Connect...", 1);
    OLED_Update();
    uint8_t retry = 5;
    _Bool mqtt_ok = 0;
    while (retry-- && !mqtt_ok) {
        mqtt_ok = (OneNet_DevLink() == 0);
        if (!mqtt_ok) HAL_Delay(3000);
    }
    if (mqtt_ok) {
        OLED_ShowStr(0, 40, "MQTT OK", 1);
        OneNET_Subscribe();
    } else {
        OLED_ShowStr(0, 40, "MQTT Fail", 1);
    }
    OLED_Update();
    HAL_Delay(500);

    /* ---------- 初始化菜单系统 ---------- */
    Menu_Init();
    sys_state = STATE_MAIN_IDLE;

    /* ---------- 所有外设初始化完毕，显示主界面 ---------- */
    OLED_CLS();
    MainInterface_Display();

    uint32_t ping_tick = HAL_GetTick();
    uint32_t data_tick = HAL_GetTick();

    /* 主循环 */
    while (1) {
        /* 自动锁定时器 */
        if (auto_lock_pending && SoftTimer_IsExpired(&auto_lock_timer)) {
            auto_lock_pending = 0;
            if (lock_flag == 1) {
                Relay_Off();
                lock_flag = 0;
                Beep_Alarm_Start(1);
                MainInterface_Display();
            }
        }
							static uint32_t last_sec_refresh = 0;
						if (HAL_GetTick() - last_sec_refresh >= 1000) {
								last_sec_refresh = HAL_GetTick();
								if (sys_state == STATE_MAIN_IDLE) {
										MainInterface_Display();
								}
						}
        /* 按键扫描 */
        uint16_t key = Key_scan();
        uint8_t long_press = Key_LongPressed();

        /* 刷卡检测 */
        CheckRfidCard();

        /* 指纹触摸中断处理 */
        if (finger_touch_flag && !admin_logged_in) {
            finger_touch_flag = 0;
            Verify_Fingerprint();
            if (sys_state == STATE_MAIN_IDLE) MainInterface_Display();
            else if (sys_state == STATE_MENU) Menu_Display();
        } else if (finger_touch_flag && admin_logged_in) {
            finger_touch_flag = 0;
        }

        /* 长按 # 管理员登录 */
        if (long_press == 13 && !admin_logged_in) {
            if (Admin_VerifyPassword()) {
                admin_logged_in = 1;
                sys_state = STATE_MENU;
                Menu_Init();
                menu_idle_timeout = HAL_GetTick() + 30000;
            } else {
                Beep_Alarm_Start(2);
                OLED_ShowStr(0, 56, "Admin Pwd Error", 1);
                OLED_Update();
                HAL_Delay(1000);
                MainInterface_Display();
            }
        }

        /* 主界面密码开锁 */
        if (sys_state == STATE_MAIN_IDLE && key == 16) {
            if (User_VerifyPassword()) Func_Unlock();
            else Beep_Alarm_Start(2);
        }

        /* 菜单界面按键 */
        if (sys_state == STATE_MENU) {
            if (!add_flag) {
                if (key != 0 && !Menu_IsReturnPending()) {
                    menu_idle_timeout = HAL_GetTick() + 30000;
                    switch (key) {
                        case 11: Menu_ProcessOp(MENU_OP_UP); break;
                        case 12: Menu_ProcessOp(MENU_OP_DOWN); break;
                        case 14: Menu_ProcessOp(MENU_OP_ENTER); break;
                        case 15: Menu_ProcessOp(MENU_OP_BACK); break;
                        default: Beep_Alarm_Start(1); break;
                    }
                }
                if (HAL_GetTick() > menu_idle_timeout && !Menu_IsReturnPending() && !add_flag) {
                    Func_AdminLogout();
                }
                Menu_Update();
            }
        }

        /* 门锁状态上报 */
        if (lock_flag != last_reported_lock_flag) {
            last_reported_lock_flag = lock_flag;
            last_report_tick = HAL_GetTick();
            OneNet_SendData();
        } else {
            if (HAL_GetTick() - last_report_tick >= 60000) {
                last_report_tick = HAL_GetTick();
                OneNet_SendData();
            }
        }

        /* MQTT 心跳 */
        if (HAL_GetTick() - ping_tick > 30000) {
            printf("Send ping...\r\n");
            OneNET_KeepAlive();
            ping_tick = HAL_GetTick();
        }

        /* 处理 ESP8266 数据 */
        uint8_t *ipd = ESP8266_GetIPD(1);
        if (ipd) {
            printf("IPD payload: %s\r\n", ipd);
            OneNet_RevPro(ipd);
        }
        if (esp8266_cnt > 0 && esp8266_cnt != esp8266_cntPre) {
            printf("Receiving: cnt=%d, cntPre=%d\r\n", esp8266_cnt, esp8266_cntPre);
        }

        /* 硬件更新 */
        Relay_Update();
        Beep_Update();

        HAL_Delay(10);
    }
}

/**
 * @brief  系统时钟配置
 */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                 | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

/**
 * @brief  UART 接收完成回调
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        if (esp8266_cnt < sizeof(esp8266_buf) - 1) esp8266_buf[esp8266_cnt++] = rx_temp;
        HAL_UART_Receive_IT(&huart2, &rx_temp, 1);
    } else if (huart->Instance == USART3) {
        HAL_UART_Receive_IT(&huart3, &rx_temp_uart3, 1);
    }
}

/**
 * @brief  错误处理
 */
void Error_Handler(void) {
    __disable_irq();
    while (1);
}