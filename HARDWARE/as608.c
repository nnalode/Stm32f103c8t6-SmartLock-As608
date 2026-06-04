#include "as608.h"
#include "delay.h"
#include "at24c02.h"
#include "onenet.h"
#include <string.h>
#include "menu_system.h"
#include "oled.h"


extern UART_HandleTypeDef huart1;
uint8_t  AS608_RX_BUF[AS608_RX_BUF_SIZE];
uint16_t AS608_RX_STA = 0;
uint32_t AS608Addr = 0xFFFFFFFF;

extern uint8_t add_flag;
volatile uint8_t finger_touch_flag = 0;
#define NOTE_PAGE   0   // ??¨¹??¡À???0??›¥????¦Ë???
uint8_t finger_slot_usage = 0;   // ????????¦Ë0~2???ID 2,3,4 // bit0~bit2 ??????ID 2,3,4???? bit0=1 ???ID=2??????
extern void MainInterface_Display(void);
#define ENROLL_RETRY_MAX    3

// as608.c ??????????????
EnrollState enroll_state = ENROLL_IDLE;
uint8_t enroll_step = 0;         // 0:????????,1:????????,2:????›¥...
uint8_t enroll_retry = 0;
uint8_t enroll_id = 0;

void LoadFingerSlotUsage(void) {
    uint8_t buf[32] = {0};
    uint8_t retry = 3;
    
    PS_Wakeup();
    HAL_Delay(200);                     // ??????
    
    while (retry--) {
        // ?????????????????????
        AS608_RX_STA = 0;
        memset(AS608_RX_BUF, 0, AS608_RX_BUF_SIZE);
        
        if (PS_ReadNotepad(NOTE_PAGE, buf) == 0x00) {
            finger_slot_usage = buf[0];
            printf("Finger slot usage loaded: 0x%02X\r\n", finger_slot_usage);
            return;
        }
        printf("ReadNotepad retry %d failed\r\n", 3 - retry);
        HAL_Delay(300);                 // ?????????
    }
    printf("WARNING: Cannot read fingerprint usage from module! Keep old value: 0x%02X\r\n", finger_slot_usage);
}

uint8_t SaveFingerSlotUsage(void) {
    uint8_t buf[32] = {0};
    buf[0] = finger_slot_usage;
    uint8_t retry = 3;
    uint8_t res;
    
    while (retry--) {
        res = PS_WriteNotepad(NOTE_PAGE, buf);
        if (res == 0x00) {
            printf("Finger slot usage saved: 0x%02X\r\n", finger_slot_usage);
            return 0;
        }
        printf("WriteNotepad retry %d failed, res=0x%02X\r\n", 3 - retry, res);
        HAL_Delay(200);
    }
    printf("ERROR: Cannot save fingerprint usage to module!\r\n");
    return 1;
}
void PS_StaGPIO_Init(void) {
	// ??? JTAG?????? SWD ????
__HAL_RCC_AFIO_CLK_ENABLE();
__HAL_AFIO_REMAP_SWJ_NOJTAG();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_AFIO_CLK_ENABLE();  // ?????? GPIO ???????
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;  // ??????
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;        // ??????????????
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void AS608_UART_Init(void) {
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
    HAL_UART_Receive_IT(&huart1, AS608_RX_BUF, AS608_RX_BUF_SIZE);
    AS608_RX_STA = 0;
}

static void MYUSART_SendData(uint8_t data) { HAL_UART_Transmit(&huart1, &data, 1, HAL_MAX_DELAY); }
static void SendHead(void) { MYUSART_SendData(0xEF); MYUSART_SendData(0x01); }
static void SendAddr(void) {
    MYUSART_SendData(AS608Addr >> 24);
    MYUSART_SendData(AS608Addr >> 16);
    MYUSART_SendData(AS608Addr >> 8);
    MYUSART_SendData(AS608Addr);
}
static void SendFlag(uint8_t flag) { MYUSART_SendData(flag); }
static void SendLength(int length) { MYUSART_SendData(length >> 8); MYUSART_SendData(length); }
static void Sendcmd(uint8_t cmd) { MYUSART_SendData(cmd); }
static void SendCheck(uint16_t check) { MYUSART_SendData(check >> 8); MYUSART_SendData(check); }

static uint8_t *WaitResponse(uint16_t timeout_ms) {
    uint32_t start = HAL_GetTick();
    while ((AS608_RX_STA & 0x8000) == 0) {
        if ((HAL_GetTick() - start) > timeout_ms) { AS608_RX_STA = 0; return NULL; }
        HAL_Delay(1);
    }
    uint16_t len = AS608_RX_STA & 0x7FFF;
    AS608_RX_STA = 0;
    HAL_UART_Receive_IT(&huart1, AS608_RX_BUF, AS608_RX_BUF_SIZE);
    if (len < 9) return NULL;
    if (AS608_RX_BUF[0] != 0xEF || AS608_RX_BUF[1] != 0x01) return NULL;
    if (AS608_RX_BUF[6] != 0x07) return NULL;
    return AS608_RX_BUF;
}

uint8_t PS_GetImage(void) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x01);
    temp = 0x01 + 0x03 + 0x01;
    SendCheck(temp);
    data = WaitResponse(2000);
    return data ? data[9] : 0xFF;
}

uint8_t PS_GenChar(uint8_t BufferID) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x04); Sendcmd(0x02);
    MYUSART_SendData(BufferID);
    temp = 0x01 + 0x04 + 0x02 + BufferID;
    SendCheck(temp);
    data = WaitResponse(2000);
    return data ? data[9] : 0xFF;
}

uint8_t PS_Match(void) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x03);
    temp = 0x01 + 0x03 + 0x03;
    SendCheck(temp);
    data = WaitResponse(2000);
    return data ? data[9] : 0xFF;
}

uint8_t PS_RegModel(void) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x05);
    temp = 0x01 + 0x03 + 0x05;
    SendCheck(temp);
    data = WaitResponse(2000);
    return data ? data[9] : 0xFF;
}

uint8_t PS_StoreChar(uint8_t BufferID, uint16_t PageID) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x06); Sendcmd(0x06);
    MYUSART_SendData(BufferID);
    MYUSART_SendData(PageID >> 8);
    MYUSART_SendData(PageID);
    temp = 0x01 + 0x06 + 0x06 + BufferID + (PageID >> 8) + (uint8_t)PageID;
    SendCheck(temp);
    data = WaitResponse(2000);
    return data ? data[9] : 0xFF;
}

uint8_t PS_DeletChar(uint16_t PageID, uint16_t N) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x07); Sendcmd(0x0C);
    MYUSART_SendData(PageID >> 8);
    MYUSART_SendData(PageID);
    MYUSART_SendData(N >> 8);
    MYUSART_SendData(N);
    temp = 0x01 + 0x07 + 0x0C
           + (PageID >> 8) + (uint8_t)PageID
           + (N >> 8) + (uint8_t)N;
    SendCheck(temp);
    data = WaitResponse(2000);
    return data ? data[9] : 0xFF;
}

uint8_t PS_Empty(void) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x0D);
    temp = 0x01 + 0x03 + 0x0D;
    SendCheck(temp);
    data = WaitResponse(2000);
    return data ? data[9] : 0xFF;
}

uint8_t PS_ReadSysPara(SysPara *p) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x03); Sendcmd(0x0F);
    temp = 0x01 + 0x03 + 0x0F;
    SendCheck(temp);
    data = WaitResponse(1000);
    if (data) {
        p->PS_max   = (data[14] << 8) + data[15];
        p->PS_level = data[17];
        p->PS_addr  = (data[18] << 24) + (data[19] << 16) + (data[20] << 8) + data[21];
        p->PS_size  = data[23];
        p->PS_N     = data[25];
        return data[9];
    }
    return 0xFF;
}

uint8_t PS_HighSpeedSearch(uint8_t BufferID, uint16_t StartPage, uint16_t PageNum, SearchResult *p) {
    uint16_t temp;
    uint8_t *data;
    SendHead(); SendAddr(); SendFlag(0x01); SendLength(0x08); Sendcmd(0x1B);
    MYUSART_SendData(BufferID);
    MYUSART_SendData(StartPage >> 8);
    MYUSART_SendData(StartPage);
    MYUSART_SendData(PageNum >> 8);
    MYUSART_SendData(PageNum);
    temp = 0x01 + 0x08 + 0x1B + BufferID
           + (StartPage >> 8) + (uint8_t)StartPage
           + (PageNum >> 8) + (uint8_t)PageNum;
    SendCheck(temp);
    data = WaitResponse(2000);
    if (data) {
        p->pageID    = (data[10] << 8) + data[11];
        p->mathscore = (data[12] << 8) + data[13];
        return data[9];
    }
    return 0xFF;
}
uint8_t PS_HandShake(uint32_t *PS_Addr) {
    uint8_t retry = 3;
    while (retry--) {
        AS608_RX_STA = 0;
        memset(AS608_RX_BUF, 0, AS608_RX_BUF_SIZE);
        HAL_UART_AbortReceive(&huart1);
        HAL_UART_Receive_IT(&huart1, AS608_RX_BUF, AS608_RX_BUF_SIZE);
        
        SendHead();
        SendAddr();
        MYUSART_SendData(0x01);
        MYUSART_SendData(0x00);
        MYUSART_SendData(0x00);
        
        uint32_t start = HAL_GetTick();
        while ((HAL_GetTick() - start) < 500) {
            if (AS608_RX_STA & 0x8000) {
                uint16_t len = AS608_RX_STA & 0x7FFF;
                // ??§¹???§Ø??????>=9??????????7???(????6)?0x07
                if (len >= 9 && AS608_RX_BUF[0]==0xEF && AS608_RX_BUF[1]==0x01 &&
                    AS608_RX_BUF[6]==0x07) {
                    *PS_Addr = (AS608_RX_BUF[2]<<24)|(AS608_RX_BUF[3]<<16)|
                               (AS608_RX_BUF[4]<<8)|AS608_RX_BUF[5];
                    AS608_RX_STA = 0;
                    return 0;   // ??????
                }
                AS608_RX_STA = 0;
                break;
            }
            HAL_Delay(10);
        }
        HAL_Delay(100);
    }
    return 1;
}

const char *EnsureMessage(uint8_t ensure) {
    switch (ensure) {
        case 0x00: return "OK";
        default: return "ERR";
    }
}

void ShowErrMessage(uint8_t ensure) { printf("%s\r\n", EnsureMessage(ensure)); }

/**
 * @brief ??????????????????§Ø??
 *        ????????¦Ç???? -> ????¦Ç?????? -> ??? -> ??????? -> ?›¥?????????ID 2~7??
 *        ????????????????????????????
 */
void Add_FR_Blocking(void) {
    uint8_t ensure;
    uint8_t step = 0;      // 0:?????,1:?????,2:???,3:???????,4:?›¥
    uint8_t retry = 0;
    uint8_t id = 0;
    char buf[24];
    
    OLED_CLS();
    OLED_ShowStr(0, 0, "Add Fingerprint", 2);
    OLED_Update();
    
    while (1) {
        switch (step) {
            case 0:  // ???????
                retry++;
                OLED_ShowStr(0, 16, "Place finger...", 1);
                OLED_Update();
                ensure = PS_GetImage();    // ???????2??
                if (ensure == 0x00) {
                    ensure = PS_GenChar(CharBuffer1);
                    if (ensure == 0x00) {
                        OLED_ShowStr(0, 32, "Finger OK, lift", 1);
                        OLED_Update();
                        HAL_Delay(500);
                        step = 1;
                        retry = 0;
                        break;
                    }
                }
                OLED_ShowStr(0, 24, EnsureMessage(ensure), 1);
                OLED_Update();
                if (retry >= 3) goto fail;
                HAL_Delay(1500);
                break;
                
            case 1:  // ???????
                retry++;
						OLED_CLS();
                OLED_ShowStr(0, 16, "Again same finger", 1);
                OLED_Update();
                ensure = PS_GetImage();
                if (ensure == 0x00) {
                    ensure = PS_GenChar(CharBuffer2);
                    if (ensure == 0x00) {
                        step = 2;
                        retry = 0;
                        break;
                    }
                }
                OLED_ShowStr(0, 24, EnsureMessage(ensure), 1);
                OLED_Update();
                if (retry >= 3) goto fail;
                HAL_Delay(1500);
                break;
                
            case 2:  // ???
                OLED_ShowStr(0, 32, "Matching...", 1);
                OLED_Update();
                if (PS_Match() == 0x00) {
                    step = 3;
                } else {
                    OLED_ShowStr(0, 32, "Not match, retry", 1);
                    OLED_Update();
                    HAL_Delay(1500);
                    step = 0;
                    retry = 0;
                }
                break;
                
            case 3:  // ???????
                if (PS_RegModel() == 0x00) {
                    step = 4;
                } else {
                    goto fail;
                }
                break;
                
            case 4:  // ?›¥????????? ID 2~7??
                LoadFingerSlotUsage();
                id = 0;
                for (int i = 0; i < 6; i++) {
                    if (!(finger_slot_usage & (1 << i))) {
                        id = i + 2;
                        finger_slot_usage |= (1 << i);
                        break;
                    }
                }
                if (id == 0) {
									OLED_CLS();
                    OLED_ShowStr(0, 16, "Storage full!", 1);
                    OLED_Update();
                    HAL_Delay(2000);
                    goto fail;
                }
                if (PS_StoreChar(CharBuffer2, id) == 0x00) {
                    SaveFingerSlotUsage();
                    OLED_CLS();
                    OLED_ShowStr(0, 16, "Enroll OK!", 2);
                    sprintf(buf, "ID: %d", id);
                    OLED_ShowStr(0, 32, buf, 1);
                    OLED_Update();
                    HAL_Delay(2000);
                    goto success;
                } else {
                    finger_slot_usage &= ~(1 << (id - 2));
                    SaveFingerSlotUsage();
                    goto fail;
                }
        }
        HAL_Delay(100);
    }
    
fail:
    OLED_CLS();
    OLED_ShowStr(0, 16, "Enroll failed!", 1);
    OLED_Update();
    HAL_Delay(2000);
    
success:
    if (admin_logged_in) {
        Menu_Display();
    } else {
        MainInterface_Display();
    }
}

/**
 * @brief §Õ???¡À???32????
 * @param NotePageNum  ???¡À?????0~7????32????
 * @param Byte32       ???32???????????
 * @return 0x00 ???
 */
// §Õ???¡À???????36???????
uint8_t PS_WriteNotepad(uint8_t NotePageNum, uint8_t *Byte32) {
    uint16_t temp = 0;
    uint8_t *data;
    SendHead();
    SendAddr();
    SendFlag(0x01);
    SendLength(36);                     // ?????????36
    Sendcmd(0x18);
    MYUSART_SendData(NotePageNum);
    for (int i = 0; i < 32; i++) {
        MYUSART_SendData(Byte32[i]);
        temp += Byte32[i];
    }
    temp = 0x01 + 36 + 0x18 + NotePageNum + temp;
    SendCheck(temp);
    data = WaitResponse(2000);          // ?I JudgeStr
    return data ? data[9] : 0xFF;
}

// ?????¡À???????0x04???????
uint8_t PS_ReadNotepad(uint8_t NotePageNum, uint8_t *Byte32) {
    uint16_t temp;
    uint8_t *data;
    SendHead();
    SendAddr();
    SendFlag(0x01);
    SendLength(0x04);                   // ?????????0x04
    Sendcmd(0x19);
    MYUSART_SendData(NotePageNum);
    temp = 0x01 + 0x04 + 0x19 + NotePageNum;
    SendCheck(temp);
    data = WaitResponse(2000);          // ?I JudgeStr
    if (data && data[9] == 0x00) {
        memcpy(Byte32, data + 10, 32);
        return 0x00;
    }
    return 0xFF;
}
void test_fingerprint(void) {
    uint8_t res;
    SearchResult sr;
    printf("=== Fingerprint Test ===\r\n");
    while (PS_Sta == 0);
    res = PS_GetImage();
    if (res != 0x00) return;
    res = PS_GenChar(CharBuffer1);
    if (res != 0x00) return;
    res = PS_HighSpeedSearch(CharBuffer1, 0, 99, &sr);
    if (res == 0x00) printf("Found ID=%d, score=%d\r\n", sr.pageID, sr.mathscore);
    else if (res == 0x09) printf("Not found\r\n");
    else printf("Error:0x%02X\r\n", res);
}
uint8_t PS_DeleteTemplate(uint16_t pageID) {
    return PS_DeletChar(pageID, 1);
}
/**
 * @brief ?? AS608 ?????????????????
 */
void PS_Sleep(void) {
    uint16_t temp;
    SendHead();
    SendAddr();
    SendFlag(0x01);
    SendLength(0x03);               // ????? + §µ??????????
    Sendcmd(0x33);                  // ???????
    temp = 0x01 + 0x03 + 0x33;
    SendCheck(temp);
    HAL_Delay(10);                  // ?????????????
    // ??????????öö???????????????? WaitResponse
}

/**
 * @brief ???? AS608????????????????
 */
void PS_Wakeup(void) {
    uint32_t addr = 0xFFFFFFFF;
    PS_HandShake(&addr);            // ????????????????
}

uint8_t PS_GetImage_Block(uint16_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < timeout_ms) {
        if (PS_GetImage() == 0x00) return 0x00;
        HAL_Delay(50);
    }
    return 0xFF;
}