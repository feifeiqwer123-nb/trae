#include "bsp_uart.h"
#include "bsp_flash.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "misc.h"
#include <string.h>

volatile uint32_t uicount_max = 0; // 总长度1024
volatile uint32_t uicount_max1 = 0; // 总长度字节

volatile uint32_t Last_Rx_Time = 0; // 记录最后接收时间
extern volatile uint32_t SysTick_Time; // 从 main.c 引入系统时间
extern volatile uint8_t Need_Write_Flash; 
volatile uint8_t IAP_Ack_Flag = 0; 
/**
 * @brief  初始化 USART1 (PA9 TX, PA10 RX)
 */
void UART1_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 开启时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    // 2. 初始化 TX (PA9)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 初始化 RX (PA10)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 初始化 NVIC 中断
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 5. 初始化 USART 参数
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &USART_InitStructure);

    // 6. 开启接收中断并使能串口
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
    
    // 初始化数组为 0xFF
    memset(RxBuffer, 0xFF, FLASH_PAGE_SIZE);
}

/**
 * @brief  初始化 USART2 (PA2 TX, PA3 RX) - 用于跟另一块 STM32 通信
 */
void UART2_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    // 1. 开启时钟 (注意 USART2 挂载在 APB1 总线上)
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // 2. 初始化 TX (PA2)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 3. 初始化 RX (PA3)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 4. 初始化 NVIC 中断 (优先级设得比串口1低一点)
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 5. 初始化 USART 参数
    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &USART_InitStructure);

    // 6. 开启接收中断并使能串口
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

/**
 * @brief  串口1 中断服务函数
 * @note   如果你的工程里有 stm32f10x_it.c，请确保那里没有重复定义这个函数
 */
void USART1_IRQHandler(void)
{
    // 【关键防线 1】：处理溢出错误 (ORE)
    // 如果因为卡顿导致没接住数据，发生了溢出错误，必须清空它，否则串口会死锁挂起
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET)
    {
        USART_ReceiveData(USART1); // 读取一次数据寄存器即可自动清除 ORE 标志
    }

    // 正常接收数据
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t rx_data = USART_ReceiveData(USART1);
        Last_Rx_Time = SysTick_Time;
        
        // 只要还没满，就往里塞
        if(RxCount < FLASH_PAGE_SIZE) {
            RxBuffer[RxCount++] = rx_data;
					 	uicount_max1++;
        }
        
        // 【关键防线 2】：凑满一页时，绝对不要在这里写 Flash！
        if (RxCount >= FLASH_PAGE_SIZE)
        {
					  uicount_max++;
            Need_Write_Flash = 1; // 立个旗子，通知 main 函数去写
        }
    }
}

void USART2_IRQHandler(void)
{
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART2);
    }

    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t rx_data = USART_ReceiveData(USART2);
        
        // 专职拦截应答信号
        if (rx_data == 0x06) {
            IAP_Ack_Flag = 1; 
        } 
        else if (rx_data == 0x15) {
            IAP_Ack_Flag = 2; 
        }
    }
}

/**
 * @brief  计算 CRC16 校验码 (Modbus 标准)
 */
uint16_t CRC16_Calculate(uint8_t *pData, uint16_t DataLen)
{
    uint16_t wCRCin = 0xFFFF;
    uint16_t wCPoly = 0xA001;
    uint8_t  wChar = 0;
    
    while (DataLen--) 
    {
        wChar = *pData++;
        wCRCin ^= wChar;
        for (int i = 0; i < 8; i++) 
        {
            if (wCRCin & 0x01) 
                wCRCin = (wCRCin >> 1) ^ wCPoly;
            else 
                wCRCin >>= 1;
        }
    }
    return wCRCin;
}

/**
 * @brief  通过串口发送一串数据
 */
void UART_SendArray(USART_TypeDef* USARTx, uint8_t *pData, uint16_t Length)
{
    for (uint16_t i = 0; i < Length; i++)
    {
        // 将数据写入发送寄存器
        USART_SendData(USARTx, pData[i]);
        // 等待发送完成标志位 (TC) 置 1
        while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);
    }
}

/**
 * @brief  将 Flash 中的文件分包带 CRC 发送给另一块 STM32
 */
/**
 * @brief  将 Flash 中的文件分包带 CRC 发送给另一块 STM32 (带失败重传机制)
 * @param  StartAddr: 文件在 Flash 中的起始地址 (例如 0x08010000)
 * @param  TotalSize: 文件的总字节数
 * @retval 1:发送成功  0:发送失败(超过重试次数)
 */
uint8_t Send_File_To_Another_STM32(uint32_t StartAddr, uint32_t TotalSize)
{
    uint32_t current_addr = StartAddr;       
    uint32_t remaining_size = TotalSize;     
    uint16_t payload_len;                    
    uint8_t  packet_buffer[256];             
    
    uint8_t  header[2] = {0x55, 0xAA};       
    uint8_t  len_bytes[2];                   
    uint8_t  crc_bytes[2];                   
    
    while (remaining_size > 0)
    {
        if (remaining_size >= 256) {
            payload_len = 256;
        } else {
            payload_len = remaining_size;
        }
        
        // 读取 Flash
        for (uint16_t i = 0; i < payload_len; i++) {
            packet_buffer[i] = *(__IO uint8_t*)(current_addr + i);
        }
        
        // 计算 CRC
        uint16_t crc_val = CRC16_Calculate(packet_buffer, payload_len);
        
        // === 重试机制开始 (最多允许当前包重发 3 次) ===
        uint8_t retry_count = 0;
        uint8_t packet_success = 0;
        
        while (retry_count < 3) 
        {
            // 发送前清空标志位
            IAP_Ack_Flag = 0; 
            
            // 依次发送帧头、长度、数据、CRC
            UART_SendArray(USART2, header, 2);
            len_bytes[0] = (uint8_t)(payload_len >> 8);
            len_bytes[1] = (uint8_t)(payload_len & 0xFF);
            UART_SendArray(USART2, len_bytes, 2);
            UART_SendArray(USART2, packet_buffer, payload_len);
            crc_bytes[0] = (uint8_t)(crc_val >> 8);
            crc_bytes[1] = (uint8_t)(crc_val & 0xFF);
            UART_SendArray(USART2, crc_bytes, 2);
            
            // 发送完毕，等待接收端回应 (带超时判断)
            uint32_t wait_start_time = SysTick_Time;
            uint8_t wait_timeout = 0;
            
            while (IAP_Ack_Flag == 0) // 死等，直到标志位变化或者超时
            {
                if ((SysTick_Time - wait_start_time) > 500) { // 500毫秒超时
                    wait_timeout = 1;
                    break;
                }
            }
            
            // 判断等待结果
            if (IAP_Ack_Flag == 1) {
                // 收到 ACK，这包发送成功！跳出重试循环，准备发下一包
                packet_success = 1;
                break; 
            } else if (IAP_Ack_Flag == 2 || wait_timeout == 1) {
                // 收到 NAK (CRC错误) 或者 超时没有回应，准备重试
                retry_count++;
                // 稍微延时一下再重发，避免两边卡死
                for(volatile int d = 0; d < 100000; d++); 
            }
        }
        
        // 如果 3 次都失败了，直接放弃整个文件的发送
        if (packet_success == 0) {
            return 0; // 发送彻底失败
        }
        // === 重试机制结束 ===
        
        // 这包确认成功了，才更新指针，继续处理剩余数据
        current_addr += payload_len;
        remaining_size -= payload_len;
    }
    
    return 1; // 所有数据发送并校验完毕，完美成功！
}


