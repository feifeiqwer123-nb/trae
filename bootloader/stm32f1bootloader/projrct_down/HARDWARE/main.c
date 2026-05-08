#include "stm32f10x.h"
#include "bsp_uart.h"
#include "bootloader.h"

volatile uint32_t SysTick_Time = 0;

void SysTick_Handler(void) {
    SysTick_Time++;
}

int main(void)
{
    SystemInit();
    SysTick_Config(SystemCoreClock / 1000); 
    
    UART1_Init(115200);

    // 记录上电初始时间
    Last_Upgrade_Time = SysTick_Time;

    while (1)
    {
        // 1. 无脑调用状态机，它会自己去缓冲区找吃的
        Bootloader_Process();
        
        // 2. 超时判断机制 (例如设定为 2000 毫秒 = 2 秒)
        // 如果距离上一次成功解析出数据包，已经过去了 2 秒
//        if ((SysTick_Time - Last_Upgrade_Time) > 2000000)
//        {
//            // 说明发送端已经发完文件了，或者压根没打算发文件
//            // 果断跳转去执行 App！
//            IAP_ExecuteApp(APP_START_ADDR);
//            
//            // 如果跳转失败（说明里面没有合法的App代码），重置时间，继续等
//            Last_Upgrade_Time = SysTick_Time;
//        }
    }
}

