#include "bsp_flash.h"

// 定义全局变量
uint8_t  RxBuffer[FLASH_PAGE_SIZE];          // 串口接收缓存区
uint16_t RxCount = 0;                        // 当前接收到的字节数
uint32_t CurrentWriteAddr = USER_FLASH_START;// 当前写入地址

/**
 * @brief  擦除并写入一页 Flash
 * @param  WriteAddr: 写入的目标地址
 * @param  pBuffer:   要写入的数据指针
 * @param  NumToWrite:要写入的字节数
 */
void Flash_Write_Page(uint32_t WriteAddr, uint8_t *pBuffer, uint16_t NumToWrite)
{
    FLASH_Status status = FLASH_COMPLETE;
    uint16_t i;
    
    // 1. 解锁 Flash
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    
    // 2. 擦除当前页 (即使只写几个字节，也必须擦除整页)
    status = FLASH_ErasePage(WriteAddr);
    if (status != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return; 
    }
    
    // 为了防止写入奇数个字节导致越界，将长度向上取偶数
    if (NumToWrite % 2 != 0) {
        NumToWrite += 1; 
    }
    
    // 3. 循环写入数据 (按半字 16-bit 写入)
    for (i = 0; i < NumToWrite; i += 2)
    {
        uint16_t HalfWord = pBuffer[i] | ((uint16_t)pBuffer[i + 1] << 8);
        status = FLASH_ProgramHalfWord(WriteAddr + i, HalfWord);
        
        if (status != FLASH_COMPLETE) {
            break;
        }
    }
    
    // 4. 上锁 Flash
    FLASH_Lock();
}

