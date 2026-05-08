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

/**
 * @brief  串口2 中断服务函数 — 解析下位机的结构化应答帧
 *
 * @details 应答帧格式 (固定 8 字节):
 *          [0x55][0xAA][CMD=0x02][LEN_H=0x00][LEN_L=0x01][Status][CRC_H][CRC_L]
 *          Status: 0x01=ACK(继续), 0x00=NAK(重发)
 */
void USART2_IRQHandler(void)
{
    // 应答帧解析状态
    typedef enum { 
        ACK_SYNC1, ACK_SYNC2, ACK_CMD, ACK_LENH, ACK_LENL, 
        ACK_STATUS, ACK_CRCH, ACK_CRCL 
    } AckState_t;
    
    static AckState_t ack_state = ACK_SYNC1;
    static uint8_t ack_frame[6];  // 存放 CMD+LEN_H+LEN_L+Status (用于 CRC 校验)
    static uint8_t ack_idx = 0;
    static uint16_t ack_crc_recv = 0;

    // 处理溢出错误
    if (USART_GetFlagStatus(USART2, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART2);
    }

    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t rx_data = USART_ReceiveData(USART2);
        
        switch (ack_state) {
            case ACK_SYNC1:
                if (rx_data == FRAME_HEADER_H) ack_state = ACK_SYNC2;
                break;
            case ACK_SYNC2:
                if (rx_data == FRAME_HEADER_L) { ack_state = ACK_CMD; ack_idx = 0; }
                else ack_state = ACK_SYNC1;
                break;
            case ACK_CMD:
                ack_frame[ack_idx++] = rx_data;  // ack_frame[0] = CMD
                if (rx_data == CMD_IAP_DATA) ack_state = ACK_LENH;
                else ack_state = ACK_SYNC1;      // CMD 不匹配，丢弃
                break;
            case ACK_LENH:
                ack_frame[ack_idx++] = rx_data;  // ack_frame[1] = LEN_H
                ack_state = ACK_LENL;
                break;
            case ACK_LENL:
                ack_frame[ack_idx++] = rx_data;  // ack_frame[2] = LEN_L
                ack_state = ACK_STATUS;
                break;
            case ACK_STATUS:
                ack_frame[ack_idx++] = rx_data;  // ack_frame[3] = Status
                ack_state = ACK_CRCH;
                break;
            case ACK_CRCH:
                ack_crc_recv = rx_data << 8;
                ack_state = ACK_CRCL;
                break;
            case ACK_CRCL:
                ack_crc_recv |= rx_data;
                {
                    // 校验 CRC: 覆盖 CMD + LEN_H + LEN_L + Status (共 4 字节)
                    uint16_t calc = CRC16_Calculate(ack_frame, 4);
                    if (calc == ack_crc_recv) {
                        // CRC 通过，根据 Status 设置标志位
                        if (ack_frame[3] == ACK_CONTINUE) {
                            IAP_Ack_Flag = 1;  // ACK: 继续下一帧
                        } else {
                            IAP_Ack_Flag = 2;  // NAK: 重发上一帧
                        }
                    }
                    // CRC 不通过则忽略该应答帧 (IAP_Ack_Flag 保持 0，会触发超时重发)
                }
                ack_state = ACK_SYNC1;  // 回到初始状态
                break;
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
 * @brief  将 Flash 中的固件文件分包发送给另一块 STM32 (新协议版本)
 *
 * @details 【帧格式】
 *          ┌──────┬──────┬──────┬───────┬───────┬───────┬───────┬───────┬───────┬──────────┬───────┬───────┐
 *          │ 0x55 │ 0xAA │ CMD  │ LEN_H │ LEN_L │ FRM_H │ FRM_L │ TOT_H │ TOT_L │ DATA     │ CRC_H │ CRC_L │
 *          └──────┴──────┴──────┴───────┴───────┴───────┴───────┴───────┴───────┴──────────┴───────┴───────┘
 *          CMD     = 0x02 (升级数据传输)
 *          LEN     = 固件数据字节数 (仅 DATA 部分, ≤256)
 *          FRM     = 当前帧序号 (从 1 开始)
 *          TOT     = 总帧数
 *          CRC16   = Modbus CRC16, 覆盖 CMD ~ DATA 最后一个字节
 *
 * @param   StartAddr  文件在 Flash 中的起始地址
 * @param   TotalSize  文件的总字节数
 * @retval  1  全部发送成功    0  某包 3 次重试均失败
 */
uint8_t Send_File_To_Another_STM32(uint32_t StartAddr, uint32_t TotalSize)
{
    uint32_t current_addr = StartAddr;
    uint32_t remaining_size = TotalSize;
    uint16_t payload_len;
    uint8_t  packet_buffer[MAX_PAYLOAD_SIZE];
    
    // 计算总帧数: 向上取整 (TotalSize / 256)
    uint16_t total_frames = (TotalSize + MAX_PAYLOAD_SIZE - 1) / MAX_PAYLOAD_SIZE;
    uint16_t current_frame = 0;
    
    uint8_t  header[2] = {FRAME_HEADER_H, FRAME_HEADER_L};
    
    // CRC 计算缓冲区: CMD(1) + LEN(2) + FRM(2) + TOT(2) + DATA(≤256) = 最大 263 字节
    uint8_t  crc_buffer[7 + MAX_PAYLOAD_SIZE];
    
    while (remaining_size > 0)
    {
        // ---- 第一步: 确定本包载荷长度 ----
        if (remaining_size >= MAX_PAYLOAD_SIZE) {
            payload_len = MAX_PAYLOAD_SIZE;
        } else {
            payload_len = remaining_size;
        }
        current_frame++;
        
        // ---- 第二步: 从 Flash 读取固件数据 ----
        for (uint16_t i = 0; i < payload_len; i++) {
            packet_buffer[i] = *(__IO uint8_t*)(current_addr + i);
        }
        
        // ---- 第三步: 组装 CRC 计算区域 (CMD ~ DATA) ----
        uint16_t crc_len = 0;
        crc_buffer[crc_len++] = CMD_IAP_DATA;                      // CMD
        crc_buffer[crc_len++] = (uint8_t)(payload_len >> 8);       // LEN_H
        crc_buffer[crc_len++] = (uint8_t)(payload_len & 0xFF);     // LEN_L
        crc_buffer[crc_len++] = (uint8_t)(current_frame >> 8);     // FRM_H
        crc_buffer[crc_len++] = (uint8_t)(current_frame & 0xFF);   // FRM_L
        crc_buffer[crc_len++] = (uint8_t)(total_frames >> 8);      // TOT_H
        crc_buffer[crc_len++] = (uint8_t)(total_frames & 0xFF);    // TOT_L
        for (uint16_t i = 0; i < payload_len; i++) {
            crc_buffer[crc_len++] = packet_buffer[i];              // DATA
        }
        uint16_t crc_val = CRC16_Calculate(crc_buffer, crc_len);
        
        // ---- 第四步: 重试机制 (最多 3 次) ----
        uint8_t retry_count = 0;
        uint8_t packet_success = 0;
        
        while (retry_count < 3)
        {
            IAP_Ack_Flag = 0;
            
            // 发送帧头 (不参与 CRC)
            UART_SendArray(USART2, header, 2);
            // 发送 CMD + LEN + FRM + TOT + DATA (参与 CRC 的部分)
            UART_SendArray(USART2, crc_buffer, crc_len);
            // 发送 CRC16 (大端序)
            uint8_t crc_bytes[2];
            crc_bytes[0] = (uint8_t)(crc_val >> 8);
            crc_bytes[1] = (uint8_t)(crc_val & 0xFF);
            UART_SendArray(USART2, crc_bytes, 2);
            
            // 等待下位机应答 (500ms 超时)
            uint32_t wait_start_time = SysTick_Time;
            uint8_t wait_timeout = 0;
            
            while (IAP_Ack_Flag == 0) {
                if ((SysTick_Time - wait_start_time) > 500) {
                    wait_timeout = 1;
                    break;
                }
            }
            
            if (IAP_Ack_Flag == 1) {
                packet_success = 1;
                break;
            } else if (IAP_Ack_Flag == 2 || wait_timeout == 1) {
                retry_count++;
                for(volatile int d = 0; d < 100000; d++);
            }
        }
        
        if (packet_success == 0) {
            return 0;  // 3 次重试失败，放弃传输
        }
        
        current_addr += payload_len;
        remaining_size -= payload_len;
    }
    
    return 1;  // 全部帧发送成功
}
