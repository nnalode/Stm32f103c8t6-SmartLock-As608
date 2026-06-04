#ifndef AT24C02_H
#define AT24C02_H

#include "main.h"

#define W_ADDR 0xA0
#define R_ADDR 0xA1
#define EEPROM_PAGE_SIZE 8
#define EEPROM_SIZE 256

#define ADMIN_PWD_ADDR      0x10
#define USER_PWD_ADDR       0x16
#define FINGER_SLOT_ADDR    0x1C
#define FINGER_COUNT_ADDR   0x1F
#define IC_CARD_LIST_ADDR   0x20
#define IC_CARD_MAX_NUM     6


#define LOG_ENTRY_SIZE      16
#define LOG_START_ADDR      0x40    // 从 0x40 开始，避开密码和IC卡区域
#define MAX_LOG_COUNT       10       // 最多 10 条日志（10×16=160字节，0x40+160=0xE0 < 0xFF）
typedef enum {
    LOG_TYPE_UNLOCK = 1,
    LOG_TYPE_CHANGE_USER_PWD = 2,
    LOG_TYPE_CHANGE_ADMIN_PWD = 3,
    LOG_TYPE_ADD_FINGER = 4,
    LOG_TYPE_DEL_FINGER = 5,
    LOG_TYPE_ADD_ICCARD = 6,
    LOG_TYPE_DEL_ICCARD = 7
} LogType;

#define UNLOCK_WAY_FINGER   1
#define UNLOCK_WAY_PASSWORD 2
#define UNLOCK_WAY_ICCARD   3
#define UNLOCK_WAY_REMOTE   4

typedef struct {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t op_type;
    uint8_t info[4];
} LogEntry;

void Log_Init(void);
uint8_t Log_Add(LogEntry *log);
void Log_ReadAll(void (*callback)(LogEntry*));
void AT24C02_WriteByte(uint16_t innerAddr, uint8_t byte);
uint8_t AT24C02_ReadByte(uint16_t innerAddr);
uint8_t AT24C02_WriteBytes(uint16_t innerAddr, uint8_t *bytes, uint16_t size);
void AT24C02_ReadBytes(uint16_t innerAddr, uint8_t *buffer, uint16_t size);
uint8_t AT24C02_EraseAll(void);
void Log_UnlockEvent(uint8_t way, uint8_t id0, uint8_t id1, uint8_t id2, uint8_t id3);

#endif