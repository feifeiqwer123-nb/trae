#include "bootloader.h"
#include "ring_buffer.h"
#include "stm32f10x_flash.h"
#include "bsp_uart.h"

extern RingBuffer_t UartRxBuffer;
volatile uint32_t Last_Upgrade_Time = 0; 
extern volatile uint32_t SysTick_Time; // 从 main.c 引入

// 定义状态机的各个状态
typedef enum {
    STATE_SYNC1, STATE_SYNC2, STATE_LEN_H, STATE_LEN_L, 
    STATE_DATA, STATE_CRC_H, STATE_CRC_L
} ParseState_t;

uint32_t CurrentFlashAddr = APP_START_ADDR;

// 复用之前的 CRC 算法
uint16_t CRC16_Calculate(uint8_t *pData, uint16_t DataLen) {
    uint16_t wCRCin = 0xFFFF;
    uint16_t wCPoly = 0xA001;
    for (uint16_t i = 0; i < DataLen; i++) {
        wCRCin ^= pData[i];
        for (int j = 0; j < 8; j++) {
            if (wCRCin & 0x01) wCRCin = (wCRCin >> 1) ^ wCPoly;
            else wCRCin >>= 1;
        }
    }
    return wCRCin;
}

// 极其优雅的边写边擦逻辑
static void Write_To_Flash(uint8_t *data, uint16_t len) {
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    // 为了防止奇数长度，向上取偶数
    if(len % 2 != 0) len += 1;
    
    for (uint16_t i = 0; i < len; i += 2) {
        // 如果当前地址刚好是页的开头，触发一次擦除
        // 这样就不需要一开始暴力擦除整个 Flash 了，收一点擦一页，极度安全
        if ((CurrentFlashAddr % FLASH_PAGE_SIZE) == 0) {
            FLASH_ErasePage(CurrentFlashAddr);
        }
        
        uint16_t half_word = data[i] | ((uint16_t)data[i+1] << 8);
        FLASH_ProgramHalfWord(CurrentFlashAddr, half_word);
        CurrentFlashAddr += 2;
    }
    FLASH_Lock();
}

/**
 * @brief  主循环中不断调用的解析函数
 */
void Bootloader_Process(void) {
    static ParseState_t state = STATE_SYNC1;
    static uint16_t payload_len = 0;
    static uint16_t data_index = 0;
    static uint8_t  payload_buffer[256];
    static uint16_t received_crc = 0;
    
    uint8_t byte;
    
    // 如果缓冲区有数据，就一直接着解析
//    while (RingBuffer_Pop(&UartRxBuffer, &byte)) {
//        switch (state) {
//            case STATE_SYNC1:
//                if (byte == 0x55) state = STATE_SYNC2;
//                break;
//                
//            case STATE_SYNC2:
//                if (byte == 0xAA) state = STATE_LEN_H;
//                else state = STATE_SYNC1; // 帧头错误，重新寻找
//                break;
//                
//            case STATE_LEN_H:
//                payload_len = byte << 8;
//                state = STATE_LEN_L;
//                break;
//                
//            case STATE_LEN_L:
//                payload_len |= byte;
//                if (payload_len > 256 || payload_len == 0) {
//                    state = STATE_SYNC1; // 长度不合法，抛弃
//                } else {
//                    data_index = 0;
//                    state = STATE_DATA;
//                }
//                break;
//                
//            case STATE_DATA:
//                payload_buffer[data_index++] = byte;
//                if (data_index >= payload_len) state = STATE_CRC_H;
//                break;
//                
//            case STATE_CRC_H:
//                received_crc = byte << 8;
//                state = STATE_CRC_L;
//                break;
//                
//						 case STATE_CRC_L:
//                received_crc |= byte;
//                
//                // 校验 CRC16
//                uint16_t calc_crc = CRC16_Calculate(payload_buffer, payload_len);
//                if (calc_crc == received_crc) {
//                    // 1. 校验通过！写入 Flash
//                    Write_To_Flash(payload_buffer, payload_len);
//                    // 2. 告诉发送端：我收到了，且校验正确！(0x06 代表 ACK)
//                    UART_SendByte(USART1, 0x06);          
//                    // 3. 更新时间戳
//                    Last_Upgrade_Time = SysTick_Time;
//                } else {
//                    // 1. CRC 错误！绝对不写 Flash。
//                    // 2. 告诉发送端：数据坏了，给我重发这一包！(0x15 代表 NAK)
//                    UART_SendByte(USART1, 0x15); 
//                }
//                
//                // 一帧解析完毕，回到初始状态等待下一帧
//                state = STATE_SYNC1; 
//                break;
//        }
//    }
		UART_SendByte(USART1, 0x15); 
}

// 跳转到 App 的标准函数
typedef void (*iapfun)(void);
void IAP_ExecuteApp(uint32_t app_addr) {
    if (((*(__IO uint32_t*)app_addr) & 0x2FFE0000) == 0x20000000) {
        __disable_irq(); // 必须关中断
        iapfun jump2app = (iapfun)*(__IO uint32_t*)(app_addr + 4);
        __set_MSP(*(__IO uint32_t*)app_addr);
        jump2app();
    }
}
