/**
 ****************************************************************
 * @file    esp8266.c
 * @brief   ESP8266 驱动：初始化、发送命令、数据收发、处理平台数据
 * @author  yl+ds
 ****************************************************************
**/
#include "stm32f103xb.h"
#include "esp8266.h"
#include "delay.h"
#include "usart.h"
#include "oled.h"
#include <string.h>
#include <stdio.h>
#include "onenet.h"
#define ESP8266_WIFI_INFO		"AT+CWJAP=\"28502\",\"15919481644\"\r\n"

unsigned char esp8266_buf[512];
short esp8266_cnt = 0, esp8266_cntPre = 0;

/**
 * @brief  清空接收缓冲区
 * @author yl+ds
 */
void ESP8266_Clear(void)
{
	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;
}

/**
 * @brief  等待接收完成（检测计数不再变化）
 * @return REV_OK-接收完成  REV_WAIT-仍在接收
 * @author yl+ds
 */
_Bool ESP8266_WaitRecive(void)
{
	if(esp8266_cnt == 0)
		return REV_WAIT;
	if(esp8266_cnt == esp8266_cntPre)
	{
		esp8266_cnt = 0;
		return REV_OK;
	}
	esp8266_cntPre = esp8266_cnt;
	return REV_WAIT;
}

/**
 * @brief  发送 AT 命令并等待期望的响应
 * @param  cmd: 命令字符串
 * @param  res: 期望的响应关键字
 * @return 0:成功  1:超时失败
 * @author yl+ds
 */
_Bool ESP8266_SendCmd(char *cmd, char *res)
{
	unsigned char timeOut = 200;
	Usart_SendString(&huart2, (unsigned char *)cmd, strlen((const char *)cmd));
	while(timeOut--)
	{
		if(ESP8266_WaitRecive() == REV_OK)
		{
			if(strstr((const char *)esp8266_buf, res) != NULL)
			{
				ESP8266_Clear();
				return 0;
			}
		}
		HAL_Delay(10);
	}
	return 1;
}

/**
 * @brief  发送数据到 ESP8266（先发 CIPSEND，收到 '>' 后发送实际数据）
 * @param  data: 数据指针
 * @param  len: 数据长度
 * @author yl+ds
 */
void ESP8266_SendData(unsigned char *data, unsigned short len)
{
	char cmdBuf[32];
	ESP8266_Clear();
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);
	if(!ESP8266_SendCmd(cmdBuf, ">"))
	{
		Usart_SendString(&huart2, data, len);
	}
}

/**
 * @brief  从 ESP8266 缓冲区提取 +IPD 后的数据
 * @param  timeOut: 等待时间（×5ms）
 * @return 数据指针  NULL:超时未收到
 * @author yl+ds
 */
unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{
	char *ptrIPD = NULL;
	do
	{
		if(ESP8266_WaitRecive() == REV_OK)
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");
			if(ptrIPD == NULL)
			{
				// 未找到 IPD 头，继续等待
			}
			else
			{
				ptrIPD = strchr(ptrIPD, ':');
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;
			}
		}
		HAL_Delay(5);
	} while(timeOut--);
	return NULL;
}

/**
 * @brief  初始化 ESP8266（AT 测试、模式、DHCP、连接 WiFi）
 * @author yl+ds
 */
void ESP8266_Init(void)
{
	ESP8266_Clear();
	printf("1. AT\r\n");
	while(ESP8266_SendCmd("AT\r\n", "OK"))
		HAL_Delay(500);
	printf("2. CWMODE\r\n");
	while(ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
		HAL_Delay(500);
	printf("3. AT+CWDHCP\r\n");
	while(ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK"))
		HAL_Delay(500);
	printf("4. CWJAP\r\n");
	while(ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
		HAL_Delay(500);
	printf("5. ESP8266 Init OK\r\n");
	HAL_Delay(500);
}

/**
 * @brief  处理 ESP8266 接收的 MQTT 数据（供主循环调用）
 * @author yl+ds
 */
void ESP8266_ProcessData(void) {
    if (esp8266_cnt > 0 && esp8266_cnt == esp8266_cntPre) {
        if (strstr((char*)esp8266_buf, "+IPD") != NULL) {
            char *data = (char*)ESP8266_GetIPD(0);
            if (data != NULL) OneNet_RevPro((unsigned char*)data);
        }
        esp8266_cnt = 0;
        esp8266_cntPre = 0;
        memset(esp8266_buf, 0, sizeof(esp8266_buf));
    }
}