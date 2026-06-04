#ifndef __AS608_H
#define __AS608_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdio.h>

#define PS_Sta   HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)

#define CharBuffer1  0x01
#define CharBuffer2  0x02

#define AS608_RX_BUF_SIZE  200

extern UART_HandleTypeDef huart1;
extern uint8_t  AS608_RX_BUF[AS608_RX_BUF_SIZE];
extern uint16_t AS608_RX_STA;
extern uint32_t AS608Addr;

typedef struct {
    uint16_t pageID;
    uint16_t mathscore;
} SearchResult;

typedef struct {
    uint16_t PS_max;
    uint8_t  PS_level;
    uint32_t PS_addr;
    uint8_t  PS_size;
    uint8_t  PS_N;
} SysPara;

typedef enum {
    ENROLL_IDLE,
    ENROLL_FIND_ID,
    ENROLL_GET_IMAGE1,
    ENROLL_GEN_CHAR1,
    ENROLL_GET_IMAGE2,
    ENROLL_GEN_CHAR2,
    ENROLL_MATCH,
    ENROLL_REG_MODEL,
    ENROLL_STORE,
    ENROLL_SUCCESS,
    ENROLL_FAIL
} EnrollState;

extern EnrollState enroll_state;
extern uint32_t enroll_timeout;
extern uint8_t enroll_id;
extern uint8_t enroll_try;
extern uint8_t enroll_step;
extern uint8_t enroll_retry;

void PS_StaGPIO_Init(void);
void AS608_UART_Init(void);
uint8_t PS_HandShake(uint32_t *PS_Addr);
uint8_t PS_GetImage(void);
uint8_t PS_GenChar(uint8_t BufferID);
uint8_t PS_Match(void);
uint8_t PS_RegModel(void);
uint8_t PS_StoreChar(uint8_t BufferID, uint16_t PageID);
uint8_t PS_DeletChar(uint16_t PageID, uint16_t N);
uint8_t PS_Empty(void);
uint8_t PS_ReadSysPara(SysPara *p);
uint8_t PS_HighSpeedSearch(uint8_t BufferID, uint16_t StartPage, uint16_t PageNum, SearchResult *p);
uint8_t PS_WriteNotepad(uint8_t NotePageNum, uint8_t *Byte32);
uint8_t PS_ReadNotepad(uint8_t NotePageNum, uint8_t *Byte32);
uint8_t PS_DeleteTemplate(uint16_t pageID);
void PS_Sleep(void);
void PS_Wakeup(void);
void LoadFingerSlotUsage(void);
uint8_t SaveFingerSlotUsage(void);
uint8_t PS_GetImage_Block(uint16_t timeout_ms);
void Verify_Fingerprint(void);
void Add_FR_Blocking(void);
const char *EnsureMessage(uint8_t ensure);

extern volatile uint8_t finger_touch_flag;
extern uint8_t finger_slot_usage;

#endif