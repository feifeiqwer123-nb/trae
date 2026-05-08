#ifndef __IAP_H
#define __IAP_H
#include "sys.h"

/*-----------------------------------------------------------------------*/
/* IAP (In-Application Programming) 模块                                 */
/* 功能: 将固件数据写入STM32内部Flash，并跳转执行                          */
/* 平台: STM32F103 (High Density, Flash页大小2KB)                         */
/*-----------------------------------------------------------------------*/

/* APP 固件存放的起始地址（必须是页对齐，即2KB的整数倍） */
#define APP_FLASH_ADDR		0x08010000		/* 偏移64KB */

/* STM32F103HD Flash 页大小 */
#define FLASH_PAGE_SIZE		2048			/* 2KB/页 */

/* APP 区域最大大小（根据芯片Flash总大小调整） */
#define APP_MAX_SIZE		(512*1024 - 64*1024)	/* 512KB - 64KB = 448KB */


/*-----------------------------------------------------------------------*/
/* 函数声明                                                               */
/*-----------------------------------------------------------------------*/

/**
 * @brief  将数据写入STM32内部Flash
 * @param  addr      : 目标Flash起始地址（必须字对齐）
 * @param  buf       : 数据缓冲区指针
 * @param  len       : 数据长度（字节数）
 * @retval 0=成功, 1=擦除失败, 2=写入失败, 3=参数错误
 */
u8 IAP_WriteFlash(u32 addr, u8 *buf, u32 len);

/**
 * @brief  擦除APP区域的Flash页
 * @param  app_size  : APP固件大小（字节数），据此计算需要擦除的页数
 * @retval 0=成功, 1=擦除失败
 */
u8 IAP_EraseFlash(u32 app_size);

/**
 * @brief  跳转到APP执行
 * @note   跳转前会检查APP起始地址处的栈指针是否合法
 *         合法范围: 0x20000000 ~ 0x20005000 (20KB SRAM)
 * @retval 无返回（成功跳转后不会返回）
 *         如果栈指针不合法，函数会返回（跳转失败）
 */
void IAP_JumpToApp(void);

#endif /* __IAP_H */
