#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include "stm32f10x.h"

// --- 用户配置区 ---
#define FLASH_PAGE_SIZE      2048//1024            // F103C8T6 每页 1KB
#define USER_FLASH_START     0x08010000      // 存放文件的起始地址 (64KB偏移处)

// 全局变量声明 (在 uart.c 和 main.c 中会用到)
extern uint8_t  RxBuffer[FLASH_PAGE_SIZE];
extern uint16_t RxCount;
extern uint32_t CurrentWriteAddr;

// 函数声明
void Flash_Write_Page(uint32_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite);

#endif /* __BSP_FLASH_H */

