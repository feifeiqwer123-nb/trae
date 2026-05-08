#include "led.h"
#include "delay.h"
#include "sys.h"
#include "usart.h"
#include "lcd.h"
#include "key.h"
#include "malloc.h"  
#include "MMC_SD.h"
#include "ff.h"			/* FatFs 文件系统头文件 */
#include "iap.h"		/* IAP Flash编程和跳转模块 */      

/*-----------------------------------------------------------------------*/
/* FatFS + IAP 例程                                                       */
/* 功能: 从SD卡读取app.bin，写入STM32内部Flash(0x08010000)，跳转执行        */
/* 平台: STM32F103 + SPI SD卡                                             */
/* 按键: KEY0 = 读取app.bin并烧写+跳转                                     */
/*-----------------------------------------------------------------------*/

/* FatFS 全局对象 */
FATFS fs;			/* 文件系统工作区 */
FIL   fil;			/* 文件对象 */


/*-----------------------------------------------------------------------*/
/* 从SD卡读取app.bin，写入内部Flash，跳转执行                               */
/*                                                                       */
/* 流程:                                                                  */
/*   1. f_open 打开 app.bin                                               */
/*   2. 获取文件大小，检查是否超出APP区域                                    */
/*   3. 擦除对应Flash页                                                    */
/*   4. 分块读取文件并逐块写入Flash                                         */
/*   5. 关闭文件，卸载文件系统                                              */
/*   6. 跳转到APP执行                                                      */
/*-----------------------------------------------------------------------*/
void IAP_LoadAndRun(const char* filename)
{
	FRESULT res;
	UINT br;
	u8 *read_buf;
	u32 file_size;
	u32 write_addr;
	u32 total_written = 0;
	u8 flash_res;

	/* 申请读取缓冲区（每次读取2048字节=1个Flash页大小，方便对齐） */
	read_buf = mymalloc(FLASH_PAGE_SIZE);
	if (read_buf == NULL) {
		printf("Memory alloc failed!\r\n");
		LCD_ShowString(60, 190, 200, 16, 16, "Memory Error!       ");
		return;
	}

	/* 1. 打开文件 */
	res = f_open(&fil, filename, FA_READ);
	if (res != FR_OK) {
		printf("f_open(\"%s\") failed: %d\r\n", filename, res);
		LCD_ShowString(60, 190, 200, 16, 16, "File Open Error!    ");
		myfree(read_buf);
		return;
	}

	/* 2. 获取文件大小并校验 */
	file_size = f_size(&fil);
	printf("\r\n=== IAP Update ===\r\n");
	printf("File: %s\r\n", filename);
	printf("Size: %lu bytes\r\n", file_size);

	if (file_size == 0) {
		printf("Error: file is empty!\r\n");
		LCD_ShowString(60, 190, 200, 16, 16, "File Empty!         ");
		f_close(&fil);
		myfree(read_buf);
		return;
	}

	if (file_size > APP_MAX_SIZE) {
		printf("Error: file too large! Max=%lu\r\n", (u32)APP_MAX_SIZE);
		LCD_ShowString(60, 190, 200, 16, 16, "File Too Large!     ");
		f_close(&fil);
		myfree(read_buf);
		return;
	}

	/* 3. 擦除Flash */
	LCD_ShowString(60, 190, 200, 16, 16, "Erasing Flash...    ");
	printf("Erasing Flash at 0x%08X ...\r\n", APP_FLASH_ADDR);

	flash_res = IAP_EraseFlash(file_size);
	if (flash_res != 0) {
		printf("Erase failed!\r\n");
		LCD_ShowString(60, 190, 200, 16, 16, "Erase Failed!       ");
		f_close(&fil);
		myfree(read_buf);
		return;
	}

	/* 4. 分块读取文件并写入Flash */
	LCD_ShowString(60, 190, 200, 16, 16, "Writing Flash...    ");
	write_addr = APP_FLASH_ADDR;

	while (1) {
		/* 读取一块数据（2048字节） */
		res = f_read(&fil, read_buf, FLASH_PAGE_SIZE, &br);
		if (res != FR_OK) {
			printf("f_read error: %d\r\n", res);
			LCD_ShowString(60, 190, 200, 16, 16, "Read Error!         ");
			f_close(&fil);
			myfree(read_buf);
			return;
		}
		if (br == 0) break;		/* 文件读取完毕 */

		/* 写入Flash */
		flash_res = IAP_WriteFlash(write_addr, read_buf, br);
		if (flash_res != 0) {
			printf("Write flash failed at 0x%08X!\r\n", write_addr);
			LCD_ShowString(60, 190, 200, 16, 16, "Write Failed!       ");
			f_close(&fil);
			myfree(read_buf);
			return;
		}

		write_addr += br;
		total_written += br;

		/* 显示进度 */
		printf("Written: %lu / %lu bytes\r\n", total_written, file_size);
	}

	/* 5. 关闭文件，释放资源 */
	f_close(&fil);
	myfree(read_buf);

	printf("Flash write complete! Total: %lu bytes\r\n", total_written);
	LCD_ShowString(60, 190, 200, 16, 16, "Write OK! Jumping...");

	/* 6. 卸载文件系统 */
	f_mount(NULL, "0:", 0);

	/* 短暂延时，让串口输出完成 */
	delay_ms(500);

	/* 7. 跳转到APP */
	printf("Jumping to APP at 0x%08X\r\n", APP_FLASH_ADDR);
	IAP_JumpToApp();

	/* 如果跳转失败会到这里 */
	printf("Jump failed! Invalid APP.\r\n");
	LCD_ShowString(60, 190, 200, 16, 16, "Jump Failed!        ");
}


/*-----------------------------------------------------------------------*/
/* 主函数                                                                 */
/*-----------------------------------------------------------------------*/
int main(void)
{
	u8 key;
	u32 sd_size;
	u8 t = 0;
	FRESULT res;

	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	delay_init();
	uart_init(9600);
	LED_Init();
	LCD_Init();
	KEY_Init();
	mem_init();

	POINT_COLOR = RED;
	LCD_ShowString(60, 50, 200, 16, 16, "Mini STM32");
	LCD_ShowString(60, 70, 200, 16, 16, "SD Card IAP Update");
	LCD_ShowString(60, 90, 200, 16, 16, "KEY0: Load app.bin");
	LCD_ShowString(60, 110, 200, 16, 16, "Mounting SD Card...");

	/* 1. 挂载文件系统 */
	res = f_mount(&fs, "0:", 1);
	if (res != FR_OK) {
		printf("f_mount error: %d\r\n", res);
		LCD_ShowString(60, 110, 200, 16, 16, "SD Mount Failed!    ");
		LCD_ShowString(60, 130, 200, 16, 16, "Error Code:");
		LCD_ShowNum(156, 130, res, 2, 16);
		while (1) {
			delay_ms(500);
			LED0 = !LED0;
		}
	}

	POINT_COLOR = BLUE;
	LCD_ShowString(60, 110, 200, 16, 16, "SD Card Mounted OK  ");

	sd_size = SD_GetSectorCount();
	LCD_ShowString(60, 130, 200, 16, 16, "SD Card Size:     MB");
	LCD_ShowNum(164, 130, sd_size >> 11, 5, 16);

	LCD_ShowString(60, 150, 200, 16, 16, "KEY0: Load app.bin");
	LCD_ShowString(60, 170, 200, 16, 16, "Ready.");

	printf("\r\n=== SD Card IAP Bootloader ===\r\n");
	printf("SD Card Size: %lu MB\r\n", sd_size >> 11);
	printf("APP Address: 0x%08X\r\n", APP_FLASH_ADDR);
	printf("Press KEY0 to load app.bin\r\n");

	/* 2. 主循环 */
	while (1) {
		key = KEY_Scan(0);
		if (key == KEY0_PRES) {
			/* 按下KEY0: 从SD卡读取app.bin → 写入Flash → 跳转执行 */
			IAP_LoadAndRun("0:app.bin");
		}

		t++;
		delay_ms(10);
		if (t == 20) {
			LED0 = !LED0;
			t = 0;
		}
	}
}
