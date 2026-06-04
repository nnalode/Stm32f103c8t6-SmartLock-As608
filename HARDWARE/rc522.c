#include "rc522.h"
#include "string.h"
#include "at24c02.h"
#include "main.h"

unsigned char CT[2];
unsigned char SN[4];
unsigned char DATA[16];
unsigned char status;
unsigned char addr=0x08;

extern SPI_HandleTypeDef hspi1;

static void delay_us(uint32_t us) {
    uint32_t i;
    for(i=0; i<us*8; i++) __NOP();
}

u8 ReadRawRC(u8 ucAddress) {
    u8 ucAddr = ((ucAddress << 1) & 0x7E) | 0x80;
    u8 ucResult = 0;
    RC522_CS_Enable();
    HAL_SPI_Transmit(&hspi1, &ucAddr, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(&hspi1, &ucResult, 1, HAL_MAX_DELAY);
    RC522_CS_Disable();
    return ucResult;
}

void WriteRawRC(u8 ucAddress, u8 ucValue) {
    u8 ucAddr = (ucAddress << 1) & 0x7E;
    u8 buf[2] = {ucAddr, ucValue};
    RC522_CS_Enable();
    HAL_SPI_Transmit(&hspi1, buf, 2, HAL_MAX_DELAY);
    RC522_CS_Disable();
}

void SetBitMask(u8 ucReg, u8 ucMask) {
    u8 ucTemp = ReadRawRC(ucReg);
    WriteRawRC(ucReg, ucTemp | ucMask);
}

void ClearBitMask(u8 ucReg, u8 ucMask) {
    u8 ucTemp = ReadRawRC(ucReg);
    WriteRawRC(ucReg, ucTemp & (~ucMask));
}

void PcdAntennaOn(void) { SetBitMask(TxControlReg, 0x03); }
void PcdAntennaOff(void) { ClearBitMask(TxControlReg, 0x03); }

void PcdReset(void) {
    RC522_Reset_Disable();
    delay_us(1);
    RC522_Reset_Enable();
    delay_us(1);
    RC522_Reset_Disable();
    delay_us(1);
    WriteRawRC(CommandReg, 0x0F);
    while(ReadRawRC(CommandReg) & 0x10);
    delay_us(1);
    WriteRawRC(ModeReg, 0x3D);
    WriteRawRC(TReloadRegL, 30);
    WriteRawRC(TReloadRegH, 0);
    WriteRawRC(TModeReg, 0x8D);
    WriteRawRC(TPrescalerReg, 0x3E);
    WriteRawRC(TxAutoReg, 0x40);
}

void M500PcdConfigISOType(u8 ucType) {
    if(ucType == 'A') {
        ClearBitMask(Status2Reg, 0x08);
        WriteRawRC(ModeReg, 0x3D);
        WriteRawRC(RxSelReg, 0x86);
        WriteRawRC(RFCfgReg, 0x7F);
        WriteRawRC(TReloadRegL, 30);
        WriteRawRC(TReloadRegH, 0);
        WriteRawRC(TModeReg, 0x8D);
        WriteRawRC(TPrescalerReg, 0x3E);
        delay_us(2);
        PcdAntennaOn();
    }
}

char PcdComMF522(u8 ucCommand, u8 *pInData, u8 ucInLenByte, u8 *pOutData, u32 *pOutLenBit) {
    char cStatus = MI_ERR;
    u8 ucIrqEn = 0x00, ucWaitFor = 0x00, ucLastBits;
    u8 ucN;
    u32 ul;
    switch(ucCommand) {
        case PCD_AUTHENT: ucIrqEn = 0x12; ucWaitFor = 0x10; break;
        case PCD_TRANSCEIVE: ucIrqEn = 0x77; ucWaitFor = 0x30; break;
        default: break;
    }
    WriteRawRC(ComIEnReg, ucIrqEn | 0x80);
    ClearBitMask(ComIrqReg, 0x80);
    WriteRawRC(CommandReg, PCD_IDLE);
    SetBitMask(FIFOLevelReg, 0x80);
    for(ul=0; ul<ucInLenByte; ul++) WriteRawRC(FIFODataReg, pInData[ul]);
    WriteRawRC(CommandReg, ucCommand);
    if(ucCommand == PCD_TRANSCEIVE) SetBitMask(BitFramingReg, 0x80);
    ul = 1000;
    do { ucN = ReadRawRC(ComIrqReg); ul--; } while((ul != 0) && !(ucN & 0x01) && !(ucN & ucWaitFor));
    ClearBitMask(BitFramingReg, 0x80);
    if(ul != 0) {
        if(!(ReadRawRC(ErrorReg) & 0x1B)) {
            cStatus = MI_OK;
            if(ucN & ucIrqEn & 0x01) cStatus = MI_NOTAGERR;
            if(ucCommand == PCD_TRANSCEIVE) {
                ucN = ReadRawRC(FIFOLevelReg);
                ucLastBits = ReadRawRC(ControlReg) & 0x07;
                if(ucLastBits) *pOutLenBit = (ucN-1)*8 + ucLastBits;
                else *pOutLenBit = ucN * 8;
                if(ucN == 0) ucN = 1;
                if(ucN > MAXRLEN) ucN = MAXRLEN;
                for(ul=0; ul<ucN; ul++) pOutData[ul] = ReadRawRC(FIFODataReg);
            }
        } else cStatus = MI_ERR;
    }
    SetBitMask(ControlReg, 0x80);
    WriteRawRC(CommandReg, PCD_IDLE);
    return cStatus;
}

char PcdRequest(u8 ucReq_code, u8 *pTagType) {
    char cStatus;
    u8 ucComMF522Buf[MAXRLEN];
    u32 ulLen;
    ClearBitMask(Status2Reg, 0x08);
    WriteRawRC(BitFramingReg, 0x07);
    SetBitMask(TxControlReg, 0x03);
    ucComMF522Buf[0] = ucReq_code;
    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 1, ucComMF522Buf, &ulLen);
    if((cStatus == MI_OK) && (ulLen == 0x10)) {
        *pTagType = ucComMF522Buf[0];
        *(pTagType+1) = ucComMF522Buf[1];
    } else cStatus = MI_ERR;
    return cStatus;
}

char PcdAnticoll(u8 *pSnr) {
    char cStatus;
    u8 uc, ucSnr_check = 0;
    u8 ucComMF522Buf[MAXRLEN];
    u32 ulLen;

    ClearBitMask(Status2Reg, 0x08);
    WriteRawRC(BitFramingReg, 0x00);
    ClearBitMask(CollReg, 0x80);

    ucComMF522Buf[0] = 0x93;
    ucComMF522Buf[1] = 0x20;
    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 2, ucComMF522Buf, &ulLen);

    printf("Anticoll status=%d, len=%lu\r\n", cStatus, ulLen);   // 调试
    if (cStatus == MI_OK) {
        for (uc = 0; uc < 4; uc++) {
            *(pSnr + uc) = ucComMF522Buf[uc];
            ucSnr_check ^= ucComMF522Buf[uc];
        }
        printf("Raw UID: %02X %02X %02X %02X  BCC: %02X\r\n", 
               ucComMF522Buf[0], ucComMF522Buf[1], 
               ucComMF522Buf[2], ucComMF522Buf[3], ucComMF522Buf[4]);
        if (ucSnr_check != ucComMF522Buf[uc]) {
            cStatus = MI_ERR;
            printf("BCC check fail! calc=%02X, recv=%02X\r\n", ucSnr_check, ucComMF522Buf[uc]);
        }
    }
    SetBitMask(CollReg, 0x80);
    return cStatus;
}

void CalulateCRC(u8 *pIndata, u8 ucLen, u8 *pOutData) {
    u8 uc, ucN;
    ClearBitMask(DivIrqReg, 0x04);
    WriteRawRC(CommandReg, PCD_IDLE);
    SetBitMask(FIFOLevelReg, 0x80);
    for(uc=0; uc<ucLen; uc++) WriteRawRC(FIFODataReg, *(pIndata+uc));
    WriteRawRC(CommandReg, PCD_CALCCRC);
    uc = 0xFF;
    do { ucN = ReadRawRC(DivIrqReg); uc--; } while((uc != 0) && !(ucN & 0x04));
    pOutData[0] = ReadRawRC(CRCResultRegL);
    pOutData[1] = ReadRawRC(CRCResultRegM);
}

char PcdSelect(u8 *pSnr) {
    char cStatus;
    u8 uc;
    u8 ucComMF522Buf[MAXRLEN];
    u32 ulLen;
    ucComMF522Buf[0] = PICC_ANTICOLL1;
    ucComMF522Buf[1] = 0x70;
    ucComMF522Buf[6] = 0;
    for(uc=0; uc<4; uc++) {
        ucComMF522Buf[uc+2] = *(pSnr+uc);
        ucComMF522Buf[6] ^= *(pSnr+uc);
    }
    CalulateCRC(ucComMF522Buf, 7, &ucComMF522Buf[7]);
    ClearBitMask(Status2Reg, 0x08);
    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 9, ucComMF522Buf, &ulLen);
    if((cStatus == MI_OK) && (ulLen == 0x18)) cStatus = MI_OK;
    else cStatus = MI_ERR;
    return cStatus;
}

char PcdAuthState(u8 ucAuth_mode, u8 ucAddr, u8 *pKey, u8 *pSnr) {
    char cStatus;
    u8 uc;
    u8 ucComMF522Buf[MAXRLEN];
    u32 ulLen;
    ucComMF522Buf[0] = ucAuth_mode;
    ucComMF522Buf[1] = ucAddr;
    for(uc=0; uc<6; uc++) ucComMF522Buf[uc+2] = *(pKey+uc);
    for(uc=0; uc<4; uc++) ucComMF522Buf[uc+8] = *(pSnr+uc);
    cStatus = PcdComMF522(PCD_AUTHENT, ucComMF522Buf, 12, ucComMF522Buf, &ulLen);
    if((cStatus != MI_OK) || (!(ReadRawRC(Status2Reg) & 0x08))) cStatus = MI_ERR;
    return cStatus;
}

char PcdRead(u8 ucAddr, u8 *pData) {
    char cStatus;
    u8 uc;
    u8 ucComMF522Buf[MAXRLEN];
    u32 ulLen;
    ucComMF522Buf[0] = PICC_READ;
    ucComMF522Buf[1] = ucAddr;
    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);
    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);
    if((cStatus == MI_OK) && (ulLen == 0x90)) {
        for(uc=0; uc<16; uc++) *(pData+uc) = ucComMF522Buf[uc];
    } else cStatus = MI_ERR;
    return cStatus;
}

char PcdWrite(u8 ucAddr, u8 *pData) {
    char cStatus;
    u8 ucComMF522Buf[MAXRLEN];
    u32 ulLen;
    ucComMF522Buf[0] = PICC_WRITE;
    ucComMF522Buf[1] = ucAddr;
    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);
    cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);
    if((cStatus != MI_OK) || (ulLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A)) cStatus = MI_ERR;
    if(cStatus == MI_OK) {
        memcpy(ucComMF522Buf, pData, 16);
        CalulateCRC(ucComMF522Buf, 16, &ucComMF522Buf[16]);
        cStatus = PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 18, ucComMF522Buf, &ulLen);
        if((cStatus != MI_OK) || (ulLen != 4) || ((ucComMF522Buf[0] & 0x0F) != 0x0A)) cStatus = MI_ERR;
    }
    return cStatus;
}

char PcdHalt(void) {
    u8 ucComMF522Buf[MAXRLEN];
    u32 ulLen;
    ucComMF522Buf[0] = PICC_HALT;
    ucComMF522Buf[1] = 0;
    CalulateCRC(ucComMF522Buf, 2, &ucComMF522Buf[2]);
    PcdComMF522(PCD_TRANSCEIVE, ucComMF522Buf, 4, ucComMF522Buf, &ulLen);
    return MI_OK;
}

void RC522_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // CS (PA4) 推挽输出，初始高
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Reset (PB0) 推挽输出，初始高
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    RC522_CS_Disable();
    RC522_Reset_Disable();     // 保证复位脚为高电平
    PcdReset();
    M500PcdConfigISOType('A');

    // 再次强制开启天线，避免内部时序问题
    SetBitMask(TxControlReg, 0x03);   // Tx1RFEn | Tx2RFEn
    HAL_Delay(100);                   // 等待射频场稳定（至少50ms，给足100ms）

    // 打印版本确认通信
    u8 ver = ReadRawRC(VersionReg);
    printf("RC522 Version: 0x%02X\r\n", ver);
    if (ver != 0x92 && ver != 0x91) {
        printf("RC522 init error!\r\n");
    }
		uint8_t txCtrl = ReadRawRC(TxControlReg);
printf("TxControlReg = 0x%02X\r\n", txCtrl);
}
void RC522_CheckVersion(void) {
    u8 ver = ReadRawRC(VersionReg);
    printf("RC522 Version: 0x%02x\r\n", ver);
}

uint8_t IC_CheckCard(uint8_t *uid) {
    uint8_t card_list[IC_CARD_MAX_NUM][4];
    AT24C02_ReadBytes(IC_CARD_LIST_ADDR, (uint8_t*)card_list, IC_CARD_MAX_NUM * 4);
    for (int i=0; i<IC_CARD_MAX_NUM; i++) {
        if (memcmp(card_list[i], uid, 4) == 0) return 1;
    }
    return 0;
}

uint8_t IC_AddCard(uint8_t *uid) {
    uint8_t card_list[IC_CARD_MAX_NUM][4];
    AT24C02_ReadBytes(IC_CARD_LIST_ADDR, (uint8_t*)card_list, IC_CARD_MAX_NUM * 4);
    for (int i=0; i<IC_CARD_MAX_NUM; i++) {
        if (memcmp(card_list[i], uid, 4) == 0) return 1;
    }
    for (int i=0; i<IC_CARD_MAX_NUM; i++) {
        uint8_t empty = 1;
        for (int j=0; j<4; j++) {
            if (card_list[i][j] != 0xFF && card_list[i][j] != 0x00) { empty=0; break; }
        }
        if (empty) {
            memcpy(card_list[i], uid, 4);
            AT24C02_WriteBytes(IC_CARD_LIST_ADDR + i*4, (uint8_t*)&card_list[i], 4);
            return 0;
        }
    }
    return 2;
}

uint8_t IC_DeleteCard(uint8_t *uid) {
    uint8_t card_list[IC_CARD_MAX_NUM][4];
    AT24C02_ReadBytes(IC_CARD_LIST_ADDR, (uint8_t*)card_list, IC_CARD_MAX_NUM * 4);
    for (int i=0; i<IC_CARD_MAX_NUM; i++) {
        if (memcmp(card_list[i], uid, 4) == 0) {
            memset(card_list[i], 0xFF, 4);
            AT24C02_WriteBytes(IC_CARD_LIST_ADDR + i*4, (uint8_t*)&card_list[i], 4);
            return 0;
        }
    }
    return 1;
}