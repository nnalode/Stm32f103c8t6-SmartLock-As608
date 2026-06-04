/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#define USART3_MAX_RECV_LEN		400					//�����ջ����ֽ���
#define USART3_MAX_SEND_LEN		400					//����ͻ����ֽ���
#define USART3_RX_EN 			1					//0,������;1,����.

#define RXBUFFERSIZE2  256     				//最大接收字节数	串口3
extern char Uart2_RxBuffer[RXBUFFERSIZE2];   	//接收数据
extern uint8_t Uart2_RxData;									//接收中断缓冲
						//接收缓冲计数

/* USER CODE END Includes */

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart2;

extern UART_HandleTypeDef huart3;

/* USER CODE BEGIN Private defines */
//#define RXBUFFERSIZE1  256     				//最大接收字节数  串口1
//char Uart1_RxBuffer[RXBUFFERSIZE1];   	//接收数据
//uint8_t Uart1_RxData;									//接收中断缓冲
//uint8_t Uart1_Rx_Cnt = 0;							//接收缓冲计数


/* USER CODE END Private defines */

void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);

/* USER CODE BEGIN Prototypes */
void Usart_SendString(UART_HandleTypeDef *huart, unsigned char *str, unsigned short len);
void UsartPrintf(UART_HandleTypeDef *huart, char *fmt, ...);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

