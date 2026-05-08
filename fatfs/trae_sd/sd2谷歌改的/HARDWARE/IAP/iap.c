#include "iap.h"
#include "stm32f10x_flash.h"
#include "usart.h"

/*-----------------------------------------------------------------------*/
/* IAP (In-Application Programming) 实现                                  */
/* 功能: STM32内部Flash擦写 + 跳转到APP执行                                */
/*-----------------------------------------------------------------------*/

/* APP跳转函数指针类型 */
typedef void (*iap_fun)(void);

/* 跳转函数指针 */
static iap_fun jump_to_app;


/**
 * @brief  擦除APP区域的Flash页
 * @param  app_size : APP固件大小（字节），据此计算需擦除的页数
 * @retval 0=成功, 1=擦除失败
 */
u8 IAP_EraseFlash(u32 app_size)
{
	u32 page_addr;
	u32 page_count;
	u32 i;
	FLASH_Status status;

	/* 计算需要擦除的页数（向上取整） */
	page_count = app_size / FLASH_PAGE_SIZE;
	if (app_size % FLASH_PAGE_SIZE) page_count++;

	/* 解锁Flash */
	FLASH_Unlock();

	for (i = 0; i < page_count; i++) {
		page_addr = APP_FLASH_ADDR + i * FLASH_PAGE_SIZE;
		status = FLASH_ErasePage(page_addr);
		if (status != FLASH_COMPLETE) {
			FLASH_Lock();
			printf("Erase page 0x%08X failed! status=%d\r\n", page_addr, status);
			return 1;
		}
	}

	FLASH_Lock();
	printf("Erased %d pages OK\r\n", page_count);
	return 0;
}


/**
 * @brief  将数据写入STM32内部Flash
 * @param  addr : 目标Flash起始地址（必须4字节对齐）
 * @param  buf  : 数据缓冲区
 * @param  len  : 数据长度（字节）
 * @retval 0=成功, 2=写入失败, 3=参数错误
 * @note   写入前必须先调用 IAP_EraseFlash() 擦除对应区域
 */
u8 IAP_WriteFlash(u32 addr, u8 *buf, u32 len)
{
	u32 i;
	u32 word;
	FLASH_Status status;

	/* 地址必须4字节对齐 */
	if (addr % 4 != 0) return 3;

	FLASH_Unlock();

	/* 按字（4字节）写入 */
	for (i = 0; i < len; i += 4) {
		/* 组装32位字（小端序） */
		if (i + 4 <= len) {
			word = (u32)buf[i] |
			       ((u32)buf[i+1] << 8) |
			       ((u32)buf[i+2] << 16) |
			       ((u32)buf[i+3] << 24);
		} else {
			/* 最后不足4字节，补0xFF */
			u32 remain = len - i;
			u32 j;
			word = 0xFFFFFFFF;
			for (j = 0; j < remain; j++) {
				word &= ~((u32)0xFF << (j * 8));
				word |= ((u32)buf[i+j] << (j * 8));
			}
		}

		status = FLASH_ProgramWord(addr + i, word);
		if (status != FLASH_COMPLETE) {
			FLASH_Lock();
			printf("Write 0x%08X failed! status=%d\r\n", addr + i, status);
			return 2;
		}
	}

	FLASH_Lock();
	return 0;
}


/**
 * @brief  跳转到APP程序执行
 * @note   跳转流程:
 *         1. 检查APP首地址处的栈指针（MSP）是否指向合法SRAM区域
 *         2. 关闭全局中断
 *         3. 设置MSP为APP的栈顶值
 *         4. 跳转到APP的复位向量（Reset_Handler）
 *
 *         APP的向量表结构（起始地址处）:
 *         [0x08010000] = 初始栈指针 (MSP)
 *         [0x08010004] = 复位向量 (Reset_Handler入口)
 */
void IAP_JumpToApp(void)
{
	u32 app_msp;	/* APP的初始栈指针 */
	u32 app_reset;	/* APP的复位向量地址 */

	/* 读取APP起始地址处的栈指针值 */
	app_msp = *(vu32*)APP_FLASH_ADDR;

	/* 检查栈指针是否在SRAM范围内（0x20000000 ~ 0x20005000, 20KB） */
	if ((app_msp & 0x2FFE0000) != 0x20000000) {
		printf("Invalid APP MSP: 0x%08X\r\n", app_msp);
		return;		/* 栈指针不合法，APP无效，不跳转 */
	}

	printf("Jumping to APP at 0x%08X ...\r\n", APP_FLASH_ADDR);

	/* 读取复位向量（APP向量表的第二个字） */
	app_reset = *(vu32*)(APP_FLASH_ADDR + 4);

	/* 将复位向量转为函数指针 */
	jump_to_app = (iap_fun)app_reset;

	/* 关闭全局中断 */
	__disable_irq();

	/* 设置主栈指针为APP的栈顶值 */
	__set_MSP(app_msp);

	/* 跳转执行（不会返回） */
	jump_to_app();
}
