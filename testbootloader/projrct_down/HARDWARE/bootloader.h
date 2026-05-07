#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#include <stdint.h>
#include <stdbool.h>

// ==================== Flash 配置 ====================
#define APP_START_ADDR    0x08010000 // App存放的起始地址
#define FLASH_PAGE_SIZE   2048       // 请根据你的芯片型号修改 (1024或2048)

// ==================== 协议相关宏定义 ====================
#define FRAME_HEADER_H     0x55       // 帧头高字节
#define FRAME_HEADER_L     0xAA       // 帧头低字节
#define CMD_IAP_DATA       0x02       // 命令字: 升级数据传输
#define ACK_CONTINUE       0x01       // 应答: 校验通过，继续下一帧
#define ACK_RESEND         0x00       // 应答: 校验失败，重发上一帧
#define MAX_PAYLOAD_SIZE   256        // 每帧最大固件数据字节数

// ==================== 全局变量声明 ====================
extern volatile uint32_t Last_Upgrade_Time; // 记录最后一次成功收到数据的时间

// ==================== 函数声明 ====================
void Bootloader_Process(void);
void Bootloader_SendResponse(uint8_t status);
void IAP_ExecuteApp(uint32_t app_addr);

#endif

