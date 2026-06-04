#ifndef __RC522_H
#define __RC522_H

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint32_t u32;

#define PCD_IDLE              0x00
#define PCD_AUTHENT           0x0E
#define PCD_RECEIVE           0x08
#define PCD_TRANSMIT          0x04
#define PCD_TRANSCEIVE        0x0C
#define PCD_RESETPHASE        0x0F
#define PCD_CALCCRC           0x03

#define PICC_REQIDL           0x26
#define PICC_REQALL           0x52
#define PICC_ANTICOLL1        0x93
#define PICC_ANTICOLL2        0x95
#define PICC_AUTHENT1A        0x60
#define PICC_AUTHENT1B        0x61
#define PICC_READ             0x30
#define PICC_WRITE            0xA0
#define PICC_HALT             0x50

#define DEF_FIFO_LENGTH       64
#define MAXRLEN               18

#define CommandReg            0x01
#define ComIEnReg             0x02
#define DivlEnReg             0x03
#define ComIrqReg             0x04
#define DivIrqReg             0x05
#define ErrorReg              0x06
#define Status1Reg            0x07
#define Status2Reg            0x08
#define FIFODataReg           0x09
#define FIFOLevelReg          0x0A
#define WaterLevelReg         0x0B
#define ControlReg            0x0C
#define BitFramingReg         0x0D
#define CollReg               0x0E
#define ModeReg               0x11
#define TxModeReg             0x12
#define RxModeReg             0x13
#define TxControlReg          0x14
#define TxAutoReg             0x15
#define TxSelReg              0x16
#define RxSelReg              0x17
#define RxThresholdReg        0x18
#define DemodReg              0x19
#define MifareReg             0x1C
#define CRCResultRegM         0x21
#define CRCResultRegL         0x22
#define ModWidthReg           0x24
#define RFCfgReg              0x26
#define GsNReg                0x27
#define CWGsCfgReg            0x28
#define ModGsCfgReg           0x29
#define TModeReg              0x2A
#define TPrescalerReg         0x2B
#define TReloadRegH           0x2C
#define TReloadRegL           0x2D
#define TCounterValueRegH     0x2E
#define TCounterValueRegL     0x2F
#define TestSel1Reg           0x31
#define TestSel2Reg           0x32
#define TestPinEnReg          0x33
#define TestPinValueReg       0x34
#define TestBusReg            0x35
#define AutoTestReg           0x36
#define VersionReg            0x37
#define AnalogTestReg         0x38
#define TestDAC1Reg           0x39
#define TestDAC2Reg           0x3A
#define TestADCReg            0x3B

#define REQ_ALL               0x52
#define KEYA                  0x60
#define KEYB                  0x61

#define MI_OK                 0
#define MI_NOTAGERR           1
#define MI_ERR                2

#define RC522_CS_Enable()     HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)
#define RC522_CS_Disable()    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)
#define RC522_Reset_Enable()  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET)
#define RC522_Reset_Disable() HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET)

extern unsigned char CT[2];
extern unsigned char SN[4];
extern unsigned char DATA[16];
extern unsigned char status;
extern unsigned char addr;

void RC522_CheckVersion(void);
char PcdHalt(void);
void RC522_Init(void);
void PcdReset(void);
void M500PcdConfigISOType(u8 type);
char PcdRequest(u8 req_code, u8 *pTagType);
char PcdAnticoll(u8 *pSnr);
char PcdSelect(u8 *pSnr);
char PcdAuthState(u8 ucAuth_mode, u8 ucAddr, u8 *pKey, u8 *pSnr);
char PcdWrite(u8 ucAddr, u8 *pData);
char PcdRead(u8 ucAddr, u8 *pData);
uint8_t IC_CheckCard(uint8_t *uid);
uint8_t IC_AddCard(uint8_t *uid);
uint8_t IC_DeleteCard(uint8_t *uid);

#endif