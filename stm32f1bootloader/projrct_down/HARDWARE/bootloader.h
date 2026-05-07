#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#include <stdint.h>
#include <stdbool.h>

#define APP_START_ADDR    0x08010000 // App存放的起始地址
#define FLASH_PAGE_SIZE   2048       // 请根据你的芯片型号修改 (1024或2048)

extern volatile uint32_t Last_Upgrade_Time; // 记录最后一次成功收到数据的时间

void Bootloader_Process(void);
void IAP_ExecuteApp(uint32_t app_addr);

#endif
