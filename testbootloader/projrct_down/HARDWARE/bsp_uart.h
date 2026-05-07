#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "stm32f10x.h"

void UART1_Init(uint32_t baudrate);
void UART_SendByte(USART_TypeDef* USARTx, uint8_t data);
void UART_SendArray(USART_TypeDef* USARTx, uint8_t *pData, uint16_t Length);

#endif



