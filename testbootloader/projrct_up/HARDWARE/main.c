#include "stm32f10x.h"
#include "bsp_uart.h"
#include "bsp_flash.h"
#include <string.h>
/*
///////////////////////////////////////////////////////////////////////////////
0x0802 0000------ox802 8fff
///////////////////////////////////////////////////////////////////////////////////
*/


// 系统的毫秒级时间戳
volatile uint32_t SysTick_Time = 0;
// 在头部声明一个全局标志位（等下在 main.c 里定义）
volatile uint8_t Need_Write_Flash; 
// 定义一个函数指针类型，用于指向 App 的复位函数
typedef void (*iapfun)(void);
iapfun jump2app;

/**
 * @brief  跳转到指定地址执行应用程序
 * @param  app_addr: 应用程序的起始地址 (例如 0x08010000)
 */
void IAP_ExecuteApp(uint32_t app_addr)
{
    // 1. 检查起始地址存放的栈顶指针是否合法
    // STM32 的 SRAM 起始地址是 0x20000000，所以栈顶指针必须落在这一段
    if (((*(__IO uint32_t*)app_addr) & 0x2FFE0000) == 0x20000000)
    {
        // 2. 极其重要：关闭所有全局中断！
        // 防止 Bootloader 的中断在 App 初始化前触发，导致 App 跑飞
        __disable_irq();

        // 3. 提取复位中断函数的地址
        // App 的第 1 个字 (app_addr) 存的是栈顶地址
        // App 的第 2 个字 (app_addr + 4) 存的就是复位函数的执行地址
        jump2app = (iapfun)*(__IO uint32_t*)(app_addr + 4);

        // 4. 设置主堆栈指针 MSP
        __set_MSP(*(__IO uint32_t*)app_addr);

        // 5. 正式跳转！(此函数调用后永远不会返回)
        jump2app();
    }
    else
    {
        // 走到这里说明 0x08010000 处的数据不是合法的 STM32 固件
        // 可以在这里通过串口打印错误提示
        // printf("Error: No valid App found at 0x%08X\r\n", app_addr);
    }
}


/**
 * @brief  系统滴答定时器中断 (每 1 毫秒触发一次)
 * @note   如果你的代码中已经有 delay.c 用到了 SysTick_Handler，
 * 请把 SysTick_Time++; 放到你原有的中断里，不要重复定义。
 */
void SysTick_Handler(void)
{
    SysTick_Time++;
}
	uint8_t i=0;
int main(void)
{

    SystemInit();
    SysTick_Config(SystemCoreClock / 1000); 
    UART1_Init(115200);
	  UART2_Init(115200); // 串口2接另外一块STM32 (别忘了加这句！)

    while (1)
    {
        // ==========================================
        // 核心逻辑 1：发现旗子举起来了，立刻写满页数据
        // ==========================================
        if (Need_Write_Flash == 1)
        {
            Flash_Write_Page(CurrentWriteAddr, RxBuffer, FLASH_PAGE_SIZE);
            CurrentWriteAddr += FLASH_PAGE_SIZE;
            
            // 写完后状态重置
            RxCount = 0;
            memset(RxBuffer, 0xFF, FLASH_PAGE_SIZE);
            Need_Write_Flash = 0; // 放下旗子
        }

        // ==========================================
        // 核心逻辑 2：处理不足 2048 字节的“尾部数据”
        // ==========================================
//				if (RxCount > 0 && Need_Write_Flash == 0) 
//				{
//						// 如果 500ms 没收到新数据，认为电脑发完了
//						if ((SysTick_Time - Last_Rx_Time) > 500) 
//						{
//								uicount_max++;
//								// 把最后剩余的数据写入 Flash
//								Flash_Write_Page(CurrentWriteAddr, RxBuffer, RxCount);
//								CurrentWriteAddr += RxCount; // 此时 CurrentWriteAddr 就是结束地址
//								
//								// ===== 在这里加入测试代码 =====
//								// 计算刚才存入 Flash 的文件总大小
//								uint32_t file_total_size = CurrentWriteAddr - USER_FLASH_START;
//								
//								// 延时一下，给点缓冲时间
//								for(volatile int d = 0; d < 1000000; d++); 
//								
//								// 调用新增的函数，把刚存进去的文件打包发出去！
//								Send_File_To_Another_STM32(USER_FLASH_START, file_total_size);
//								// ============================
//								
//								// 状态重置，准备接下一个文件
//								CurrentWriteAddr = USER_FLASH_START; // 这里我改成了复位到起始地址
//								RxCount = 0;
//								memset(RxBuffer, 0xFF, FLASH_PAGE_SIZE);
//						}
//				}
					
					if(i==0)   
					{
						Send_File_To_Another_STM32(USER_FLASH_START, 58532);//58532
						i=1;   
					}

    }
}

