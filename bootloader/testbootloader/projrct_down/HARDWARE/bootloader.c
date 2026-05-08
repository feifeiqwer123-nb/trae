#include "bootloader.h"
#include "ring_buffer.h"
#include "stm32f10x_flash.h"
#include "bsp_uart.h"

extern RingBuffer_t UartRxBuffer;
volatile uint32_t Last_Upgrade_Time = 0; 
extern volatile uint32_t SysTick_Time; // 从 main.c 引入

// 定义状态机的各个状态 (新协议: 帧头+CMD+LEN+FRM+TOT+DATA+CRC)
typedef enum {
    STATE_SYNC1,    // 等待帧头 0x55
    STATE_SYNC2,    // 等待帧头 0xAA
    STATE_CMD,      // 接收 CMD 字节
    STATE_LEN_H,    // 接收长度高字节
    STATE_LEN_L,    // 接收长度低字节
    STATE_FRM_H,    // 接收帧序号高字节
    STATE_FRM_L,    // 接收帧序号低字节
    STATE_TOT_H,    // 接收总帧数高字节
    STATE_TOT_L,    // 接收总帧数低字节
    STATE_DATA,     // 接收固件数据
    STATE_CRC_H,    // 接收 CRC 高字节
    STATE_CRC_L     // 接收 CRC 低字节
} ParseState_t;

uint32_t CurrentFlashAddr = APP_START_ADDR;

// Modbus CRC16 算法
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

// 边写边擦 Flash 逻辑
static void Write_To_Flash(uint8_t *data, uint16_t len) {
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    // 为了防止奇数长度，向上取偶数
    if(len % 2 != 0) len += 1;
    
    for (uint16_t i = 0; i < len; i += 2) {
        // 如果当前地址刚好是页的开头，触发一次擦除
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
 * @brief  组装并发送结构化应答帧给上位机
 *
 * @details 应答帧格式 (固定 8 字节):
 *          [0x55][0xAA][CMD=0x02][LEN_H=0x00][LEN_L=0x01][Status][CRC_H][CRC_L]
 *          CRC 覆盖 CMD + LEN_H + LEN_L + Status (共 4 字节)
 *
 * @param   status  ACK_CONTINUE(0x01) 或 ACK_RESEND(0x00)
 */
void Bootloader_SendResponse(uint8_t status) {
    uint8_t resp[8];
    resp[0] = FRAME_HEADER_H;        // 帧头
    resp[1] = FRAME_HEADER_L;
    resp[2] = CMD_IAP_DATA;           // CMD = 0x02
    resp[3] = 0x00;                   // LEN_H = 0
    resp[4] = 0x01;                   // LEN_L = 1 (Status 只有 1 字节)
    resp[5] = status;                 // Status: 0x01=ACK, 0x00=NAK
    
    // CRC 覆盖 resp[2..5] = CMD + LEN_H + LEN_L + Status
    uint16_t crc = CRC16_Calculate(&resp[2], 4);
    resp[6] = (uint8_t)(crc >> 8);    // CRC_H
    resp[7] = (uint8_t)(crc & 0xFF);  // CRC_L
    
    UART_SendArray(USART1, resp, 8);
}

/**
 * @brief  Bootloader 协议解析主函数 (在 main 的 while(1) 中不断调用)
 *
 * @details 新协议帧格式:
 *          [0x55][0xAA][CMD][LEN_H][LEN_L][FRM_H][FRM_L][TOT_H][TOT_L][DATA...][CRC_H][CRC_L]
 *          - CMD = 0x02 (升级数据传输)
 *          - LEN = 固件数据长度 (仅 DATA 部分, ≤256)
 *          - FRM = 当前帧序号 (从 1 开始)
 *          - TOT = 总帧数
 *          - CRC = Modbus CRC16, 覆盖 CMD ~ DATA 最后一个字节
 *
 *          收到最后一帧 (FRM == TOT) 并校验通过后，自动跳转 App。
 */
void Bootloader_Process(void) {
    // static 变量: 跨调用保持状态
    static ParseState_t state = STATE_SYNC1;
    static uint8_t  cmd = 0;                    // 接收到的 CMD
    static uint16_t payload_len = 0;            // 固件数据长度 (LEN 字段)
    static uint16_t frame_num = 0;              // 当前帧序号
    static uint16_t total_frames = 0;           // 总帧数
    static uint16_t data_index = 0;             // 已接收数据字节计数
    static uint8_t  payload_buffer[MAX_PAYLOAD_SIZE]; // 固件数据暂存区
    static uint16_t received_crc = 0;           // 帧中携带的 CRC16
    
    // CRC 验证缓冲区: CMD(1) + LEN(2) + FRM(2) + TOT(2) + DATA(≤256) = 最大 263 字节
    static uint8_t  crc_check_buf[7 + MAX_PAYLOAD_SIZE];
    static uint16_t crc_check_len = 0;
    
    uint8_t byte;
    
    while (RingBuffer_Pop(&UartRxBuffer, &byte)) {
        switch (state) {
            
            // ========== 帧头同步 ==========
            case STATE_SYNC1:
                if (byte == FRAME_HEADER_H) state = STATE_SYNC2;
                break;
                
            case STATE_SYNC2:
                if (byte == FRAME_HEADER_L) {
                    state = STATE_CMD;
                    crc_check_len = 0;  // 重置 CRC 缓冲区
                }
                else state = STATE_SYNC1;
                break;
            
            // ========== CMD 字段 ==========
            case STATE_CMD:
                cmd = byte;
                crc_check_buf[crc_check_len++] = byte;  // CMD 参与 CRC
                if (cmd == CMD_IAP_DATA) {
                    state = STATE_LEN_H;
                } else {
                    state = STATE_SYNC1;  // 不认识的命令，丢弃
                }
                break;
            
            // ========== 长度字段 (Date_Len, 仅固件数据长度) ==========
            case STATE_LEN_H:
                payload_len = byte << 8;
                crc_check_buf[crc_check_len++] = byte;  // LEN_H 参与 CRC
                state = STATE_LEN_L;
                break;
                
            case STATE_LEN_L:
                payload_len |= byte;
                crc_check_buf[crc_check_len++] = byte;  // LEN_L 参与 CRC
                if (payload_len > MAX_PAYLOAD_SIZE || payload_len == 0) {
                    state = STATE_SYNC1;  // 长度不合法
                } else {
                    state = STATE_FRM_H;
                }
                break;
            
            // ========== 帧序号 (Date1, 2 字节大端序) ==========
            case STATE_FRM_H:
                frame_num = byte << 8;
                crc_check_buf[crc_check_len++] = byte;  // FRM_H 参与 CRC
                state = STATE_FRM_L;
                break;
                
            case STATE_FRM_L:
                frame_num |= byte;
                crc_check_buf[crc_check_len++] = byte;  // FRM_L 参与 CRC
                state = STATE_TOT_H;
                break;
            
            // ========== 总帧数 (Date2, 2 字节大端序) ==========
            case STATE_TOT_H:
                total_frames = byte << 8;
                crc_check_buf[crc_check_len++] = byte;  // TOT_H 参与 CRC
                state = STATE_TOT_L;
                break;
                
            case STATE_TOT_L:
                total_frames |= byte;
                crc_check_buf[crc_check_len++] = byte;  // TOT_L 参与 CRC
                data_index = 0;
                state = STATE_DATA;
                break;
            
            // ========== 固件数据 (Date3~DateN) ==========
            case STATE_DATA:
                payload_buffer[data_index++] = byte;
                crc_check_buf[crc_check_len++] = byte;  // DATA 参与 CRC
                if (data_index >= payload_len) state = STATE_CRC_H;
                break;
            
            // ========== CRC16 校验 ==========
            case STATE_CRC_H:
                received_crc = byte << 8;
                state = STATE_CRC_L;
                break;
                
            case STATE_CRC_L:
                received_crc |= byte;
                {
                    // CRC 校验: 覆盖 CMD + LEN + FRM + TOT + DATA
                    uint16_t calc_crc = CRC16_Calculate(crc_check_buf, crc_check_len);
                    if (calc_crc == received_crc) {
                        // ✅ CRC 校验通过
                        Write_To_Flash(payload_buffer, payload_len);
                        Bootloader_SendResponse(ACK_CONTINUE);
                        Last_Upgrade_Time = SysTick_Time;
                        
                        // 检查是否为最后一帧
                        if (frame_num == total_frames) {
                            // 所有帧接收完毕，跳转到 App
                            IAP_ExecuteApp(APP_START_ADDR);
                            // 如果跳转失败 (App 无效)，继续留在 Bootloader
                        }
                    } else {
                        // ❌ CRC 校验失败
                        Bootloader_SendResponse(ACK_RESEND);
                    }
                }
                state = STATE_SYNC1;
                break;
        }
    }
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

