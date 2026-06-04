#ifndef _ONENET_H_
#define _ONENET_H_

#include "Common.h"
_Bool OneNET_RegisterDevice(void);
_Bool OneNet_DevLink(void);
void OneNet_SendData(void);
void OneNET_Subscribe(void);
void OneNet_RevPro(unsigned char *cmd);
void OneNET_KeepAlive(void);
void Log_RecordAndUpload(uint8_t op_type, uint8_t *info, uint8_t info_len);
#endif