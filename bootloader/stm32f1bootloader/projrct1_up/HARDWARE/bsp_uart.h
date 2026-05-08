#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "stm32f10x.h"

// 记录最后一次接收到数据的时间
extern volatile uint32_t Last_Rx_Time;

// 串口初始化函数
void UART1_Init(uint32_t baudrate);

// ================= 新增的代码声明 =================
uint16_t CRC16_Calculate(uint8_t *pData, uint16_t DataLen);
void UART_SendArray(USART_TypeDef* USARTx, uint8_t *pData, uint16_t Length);
uint8_t Send_File_To_Another_STM32(uint32_t StartAddr, uint32_t TotalSize);
// 增加串口2的初始化声明
void UART2_Init(uint32_t baudrate);
extern volatile uint32_t uicount_max ; 
extern volatile uint32_t uicount_max1 ; 

#endif /* __BSP_UART_H */
