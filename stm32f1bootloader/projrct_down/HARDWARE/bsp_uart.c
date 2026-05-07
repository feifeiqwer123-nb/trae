#include "bsp_uart.h"
#include "ring_buffer.h"

// 实例化一个全局的环形缓冲区
RingBuffer_t UartRxBuffer;

void UART1_Init(uint32_t baudrate)
{
    // ... [代码省略] ... 
    // 这里完全复用你之前发送端的 GPIO、NVIC 和 USART 初始化代码
    // 初始化时记得顺便初始化一下环形缓冲区
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
	
    RingBuffer_Init(&UartRxBuffer);
}

void USART1_IRQHandler(void)
{
    // 处理溢出错误
    if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
        USART_ReceiveData(USART1);
    }

    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t rx_data = USART_ReceiveData(USART1);
        
        // 【极速操作】：将数据直接丢进缓冲区就退出中断
        RingBuffer_Push(&UartRxBuffer, rx_data);
    }
}

void UART_SendByte(USART_TypeDef* USARTx, uint8_t data)
{
    // 将数据写入发送寄存器
    USART_SendData(USARTx, data);
    // 等待发送完成标志位 (TC) 置 1
    while (USART_GetFlagStatus(USARTx, USART_FLAG_TC) == RESET);
}

