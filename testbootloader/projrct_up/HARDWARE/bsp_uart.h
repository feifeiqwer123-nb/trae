#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "stm32f10x.h"

// ==================== 协议相关宏定义 ====================
#define FRAME_HEADER_H     0x55       // 帧头高字节
#define FRAME_HEADER_L     0xAA       // 帧头低字节
#define CMD_IAP_DATA       0x02       // 命令字: 升级数据传输
#define ACK_CONTINUE       0x01       // 下位机应答: 校验通过，继续下一帧
#define ACK_RESEND         0x00       // 下位机应答: 校验失败，重发上一帧
#define MAX_PAYLOAD_SIZE   256        // 每帧最大固件数据字节数

// ==================== 全局变量声明 ====================
// 记录最后一次接收到数据的时间
extern volatile uint32_t Last_Rx_Time;
extern volatile uint32_t uicount_max;
extern volatile uint32_t uicount_max1;

// ==================== 函数声明 ====================
// 串口初始化
void UART1_Init(uint32_t baudrate);
void UART2_Init(uint32_t baudrate);

// 底层通信
uint16_t CRC16_Calculate(uint8_t *pData, uint16_t DataLen);
void UART_SendArray(USART_TypeDef* USARTx, uint8_t *pData, uint16_t Length);

// 核心发送函数
uint8_t Send_File_To_Another_STM32(uint32_t StartAddr, uint32_t TotalSize);

#endif /* __BSP_UART_H */

