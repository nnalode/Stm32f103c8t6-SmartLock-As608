#include "at24c02.h"
#include <string.h>
#include <stdio.h>
#include "ds1302.h"
extern I2C_HandleTypeDef hi2c1;



void AT24C02_WriteByte(uint16_t innerAddr, uint8_t byte) {
    uint8_t retry = 3;
    while (retry--) {
        HAL_I2C_Mem_Write(&hi2c1, W_ADDR, (uint8_t)innerAddr, I2C_MEMADD_SIZE_8BIT, &byte, 1, 500);
        HAL_Delay(10);
        if (AT24C02_ReadByte(innerAddr) == byte) break;
    }
}

uint8_t AT24C02_ReadByte(uint16_t innerAddr) {
    if (innerAddr >= EEPROM_SIZE) return 0xFF;
    uint8_t byte;
    HAL_I2C_Mem_Read(&hi2c1, R_ADDR, (uint8_t)innerAddr, I2C_MEMADD_SIZE_8BIT, &byte, 1, 1000);
    return byte;
}

void AT24C02_ReadBytes(uint16_t innerAddr, uint8_t *buffer, uint16_t size) {
    if (innerAddr >= EEPROM_SIZE) return;
    if (innerAddr + size > EEPROM_SIZE) size = EEPROM_SIZE - innerAddr;
    if (size == 0) return;
    for (uint16_t i = 0; i < size; i++) {
        buffer[i] = AT24C02_ReadByte(innerAddr + i);
    }
}

uint8_t AT24C02_WriteBytes(uint16_t innerAddr, uint8_t *bytes, uint16_t size) {
    if (innerAddr >= EEPROM_SIZE) return 1;
    if (innerAddr + size > EEPROM_SIZE) size = EEPROM_SIZE - innerAddr;
    if (size == 0) return 1;
    for (uint16_t i = 0; i < size; i++) {
        AT24C02_WriteByte(innerAddr + i, bytes[i]);
        HAL_Delay(5);   // 等待内部写周期
    }
    return 0;
}
// ==================== ????洢??? ====================
static uint8_t log_write_idx = 0;   // ???д??λ??
static uint8_t log_count = 0;       // ?????????

void Log_Init(void)
{
    log_write_idx = AT24C02_ReadByte(LOG_START_ADDR);
    log_count = AT24C02_ReadByte(LOG_START_ADDR + 1);
    
    if (log_write_idx >= MAX_LOG_COUNT) log_write_idx = 0;
    if (log_count > MAX_LOG_COUNT) log_count = 0;
}

/**
 * @brief  添加一条日志（循环队列，满后覆盖最旧的）
 * @param  log: 日志条目指针
 * @return 0:成功  1:写入验证失败
 * @author yl+ds
 */
uint8_t Log_Add(LogEntry *log) {
    uint16_t addr = LOG_START_ADDR + 2 + log_write_idx * LOG_ENTRY_SIZE;
    uint8_t buf[LOG_ENTRY_SIZE];
    uint8_t res = 0;
    
    // 检查地址是否超出 EEPROM 容量
    if (addr + LOG_ENTRY_SIZE > EEPROM_SIZE) {
        printf("Log address overflow 0x%04X!\r\n", addr);
        return 1;
    }
    
    // 准备要写入的数据缓冲区
    buf[0] = log->year;
    buf[1] = log->month;
    buf[2] = log->day;
    buf[3] = log->hour;
    buf[4] = log->minute;
    buf[5] = log->second;
    buf[6] = log->op_type;
    for (int i = 0; i < 4; i++)
        buf[7 + i] = log->info[i];
    
    // 逐字节写入并验证
    for (int i = 0; i < LOG_ENTRY_SIZE; i++) {
        AT24C02_WriteByte(addr + i, buf[i]);
        uint8_t verify = AT24C02_ReadByte(addr + i);
        if (verify != buf[i]) {
            printf("Log write error at addr 0x%04X: wrote 0x%02X, read 0x%02X\r\n", addr + i, buf[i], verify);
            res = 1;
        }
    }
    
    if (res == 0) {
        // 循环队列：移动写指针
        log_write_idx = (log_write_idx + 1) % MAX_LOG_COUNT;
        
        // 队列未满时增加计数，满了就覆盖（计数保持 MAX_LOG_COUNT）
        if (log_count < MAX_LOG_COUNT) {
            log_count++;
        }
        
        // 更新索引和计数到 EEPROM
        AT24C02_WriteByte(LOG_START_ADDR, log_write_idx);
        HAL_Delay(5);
        AT24C02_WriteByte(LOG_START_ADDR + 1, log_count);
        HAL_Delay(5);
        
        // 验证索引和计数是否写入成功
        if (AT24C02_ReadByte(LOG_START_ADDR) != log_write_idx ||
            AT24C02_ReadByte(LOG_START_ADDR + 1) != log_count) {
            printf("Log index/count update failed!\r\n");
            return 1;
        }
        
        printf("Log added: type=%d, time=20%02d-%02d-%02d %02d:%02d:%02d, idx=%d, count=%d\r\n",
               log->op_type, log->year, log->month, log->day,
               log->hour, log->minute, log->second,
               log_write_idx, log_count);
    } else {
        printf("Log write verification failed!\r\n");
    }
    
    return res;
}

void Log_ReadAll(void (*callback)(LogEntry*))
{
    if (!callback) return;
    uint8_t start_idx = 0;
    uint8_t entries = log_count;
    
    if (log_count == MAX_LOG_COUNT) {
        start_idx = log_write_idx;
        entries = MAX_LOG_COUNT;
    } else {
        start_idx = 0;
        entries = log_count;
    }
    
    for (uint8_t i = 0; i < entries; i++) {
        uint8_t idx = (start_idx + i) % MAX_LOG_COUNT;
        uint16_t addr = LOG_START_ADDR + 2 + idx * LOG_ENTRY_SIZE;
        LogEntry log;
        log.year   = AT24C02_ReadByte(addr);
        log.month  = AT24C02_ReadByte(addr + 1);
        log.day    = AT24C02_ReadByte(addr + 2);
        log.hour   = AT24C02_ReadByte(addr + 3);
        log.minute = AT24C02_ReadByte(addr + 4);
        log.second = AT24C02_ReadByte(addr + 5);
        log.op_type= AT24C02_ReadByte(addr + 6);
        for (int j = 0; j < 4; j++)
            log.info[j] = AT24C02_ReadByte(addr + 7 + j);
        callback(&log);
    }
}
/**
 * @brief 擦除整个 AT24C02 EEPROM（所有字节写为 0xFF）
 * @return 0:成功, 1:失败
 */
uint8_t AT24C02_EraseAll(void)
{
    uint8_t buffer[EEPROM_PAGE_SIZE];  // 16 字节页面缓冲区
    uint16_t addr;
    
    // 填充缓冲区为 0xFF
    for (uint8_t i = 0; i < EEPROM_PAGE_SIZE; i++) {
        buffer[i] = 0xFF;
    }
    
    // 按页写入整个 EEPROM
    for (addr = 0; addr < EEPROM_SIZE; addr += EEPROM_PAGE_SIZE) {
        if (AT24C02_WriteBytes(addr, buffer, EEPROM_PAGE_SIZE) != 0) {
            printf("Erase failed at addr 0x%04X\r\n", addr);
            return 1;
        }
        HAL_Delay(10);  // 等待页面写入完成
    }
    
    // 验证：随机抽查几个地址
    uint8_t test_addr[] = {0, 63, 127, 191, 255};
    for (uint8_t i = 0; i < sizeof(test_addr); i++) {
        if (AT24C02_ReadByte(test_addr[i]) != 0xFF) {
            printf("Verification failed at addr 0x%02X\r\n", test_addr[i]);
            return 1;
        }
    }
    
    printf("AT24C02 erased successfully (all bytes set to 0xFF)\r\n");
    return 0;
}
/**
 * @brief  记录开锁事件（自动获取当前时间并添加到日志）
 */
void Log_UnlockEvent(uint8_t way, uint8_t id0, uint8_t id1, uint8_t id2, uint8_t id3) {
    LogEntry log;
    DS1302_GetTime();
    log.year   = Time[YEAR];
    log.month  = Time[MONTH];
    log.day    = Time[DATE];
    log.hour   = Time[HOUR];
    log.minute = Time[MINUTE];
    log.second = Time[SECOND];
    log.op_type = LOG_TYPE_UNLOCK;
    log.info[0] = way;
    log.info[1] = id0;
    log.info[2] = id1;
    log.info[3] = id2;
    Log_Add(&log);
}