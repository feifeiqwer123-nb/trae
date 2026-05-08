/*-----------------------------------------------------------------------*/
/* FatFs 底层磁盘IO适配层 - 桥接 SD 卡 SPI 驱动                          */
/* 适用平台: STM32F103 + SPI SD卡                                        */
/*-----------------------------------------------------------------------*/

#include "ff.h"			/* FatFs 头文件，包含类型定义 */
#include "diskio.h"		/* 磁盘IO接口声明 */
#include "MMC_SD.h"		/* SD卡底层SPI驱动 */

/* MMC_SD.C 中定义但头文件未声明的函数 */
extern u8 SD_Select(void);
extern void SD_DisSelect(void);

/* 物理驱动器编号定义 */
#define SD_CARD		0	/* SD卡映射为驱动器0 */


/*-----------------------------------------------------------------------*/
/* 获取磁盘驱动器状态                                                     */
/*                                                                       */
/* 功能：查询指定驱动器当前是否已初始化、是否有介质、是否写保护。            */
/*       FatFs 在挂载和每次操作前会调用此函数检查驱动器状态。               */
/* 参数：pdrv - 物理驱动器编号（本工程中 0 = SD卡）                       */
/* 返回：DSTATUS 状态位掩码（0=正常，STA_NOINIT=未初始化）                */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive number to identify the drive */
)
{
	if (pdrv != SD_CARD) return STA_NOINIT;

	/* SD_Type 是 MMC_SD 驱动中的全局变量，
	   初始化成功后会被设为具体的卡类型值（非0） */
	if (SD_Type != 0) return 0;		/* 驱动器正常 */

	return STA_NOINIT;				/* 未初始化 */
}



/*-----------------------------------------------------------------------*/
/* 初始化磁盘驱动器                                                       */
/*                                                                       */
/* 功能：对指定的物理驱动器执行硬件初始化。                                 */
/*       FatFs 在 f_mount() 挂载时，如果驱动器尚未初始化，会调用此函数。     */
/*       内部调用 SD_Initialize() 完成 SPI 配置、SD卡识别和类型检测。       */
/* 参数：pdrv - 物理驱动器编号                                            */
/* 返回：DSTATUS 状态（0=成功，STA_NOINIT=失败）                          */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive number to identify the drive */
)
{
	u8 res;

	if (pdrv != SD_CARD) return STA_NOINIT;

	res = SD_Initialize();	/* 调用底层SD卡初始化 */

	if (res == 0) return 0;			/* 初始化成功 */

	return STA_NOINIT;				/* 初始化失败 */
}



/*-----------------------------------------------------------------------*/
/* 读取扇区数据                                                           */
/*                                                                       */
/* 功能：从指定物理驱动器读取一个或多个扇区的数据到缓冲区。                  */
/*       FatFs 在 f_read() 等文件读操作时会调用此函数。                     */
/*       内部调用 SD_ReadDisk() 通过 SPI 从 SD 卡读取 512 字节/扇区。      */
/* 参数：pdrv   - 物理驱动器编号                                          */
/*       buff   - 接收数据的缓冲区指针                                    */
/*       sector - 起始扇区号（LBA）                                       */
/*       count  - 要读取的扇区数量                                        */
/* 返回：RES_OK=成功，RES_ERROR=读取失败，RES_PARERR=参数错误              */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive number to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	u8 res;

	if (pdrv != SD_CARD) return RES_PARERR;

	res = SD_ReadDisk(buff, sector, (u8)count);

	if (res == 0) return RES_OK;

	return RES_ERROR;
}



/*-----------------------------------------------------------------------*/
/* 写入扇区数据                                                           */
/*                                                                       */
/* 功能：将缓冲区中的数据写入指定物理驱动器的一个或多个扇区。                */
/*       FatFs 在 f_write()、f_sync() 等文件写操作时会调用此函数。          */
/*       内部调用 SD_WriteDisk() 通过 SPI 向 SD 卡写入 512 字节/扇区。     */
/* 参数：pdrv   - 物理驱动器编号                                          */
/*       buff   - 待写入数据的缓冲区指针                                  */
/*       sector - 起始扇区号（LBA）                                       */
/*       count  - 要写入的扇区数量                                        */
/* 返回：RES_OK=成功，RES_ERROR=写入失败，RES_PARERR=参数错误              */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive number to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	u8 res;

	if (pdrv != SD_CARD) return RES_PARERR;

	res = SD_WriteDisk((u8*)buff, sector, (u8)count);

	if (res == 0) return RES_OK;

	return RES_ERROR;
}

#endif


/*-----------------------------------------------------------------------*/
/* 杂项磁盘控制命令                                                       */
/*                                                                       */
/* 功能：执行除读写以外的磁盘控制操作。FatFs 在以下场景调用此函数：          */
/*       - CTRL_SYNC:      确保写缓冲已刷新到介质（f_sync/f_close时）      */
/*       - GET_SECTOR_COUNT: 获取扇区总数（f_mkfs/f_getfree时）            */
/*       - GET_SECTOR_SIZE:  获取扇区大小（FF_MAX_SS != FF_MIN_SS 时）     */
/*       - GET_BLOCK_SIZE:   获取擦除块大小（f_mkfs时优化对齐）            */
/* 参数：pdrv - 物理驱动器编号                                            */
/*       cmd  - 控制命令代码                                              */
/*       buff - 参数/结果缓冲区指针                                       */
/* 返回：RES_OK=成功，RES_ERROR=操作失败，RES_PARERR=参数错误              */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive number (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res = RES_PARERR;

	if (pdrv != SD_CARD) return RES_PARERR;

	switch (cmd) {
	case CTRL_SYNC:
		/* 确保所有挂起的写操作已完成 */
		if (SD_Select() == 0) {   
			SD_DisSelect();   
			res = RES_OK;
		} else {
			res = RES_ERROR;
		}
		break;

	case GET_SECTOR_COUNT:
		/* 返回SD卡总扇区数 */
		*(DWORD*)buff = SD_GetSectorCount();
		res = RES_OK;
		break;

	case GET_SECTOR_SIZE:
		/* SD卡扇区大小固定512字节 */
		*(WORD*)buff = 512;
		res = RES_OK;
		break;

	case GET_BLOCK_SIZE:
		/* 擦除块大小（单位：扇区），SD卡一般为8扇区 */
		*(DWORD*)buff = 8;
		res = RES_OK;
		break;

	default:
		res = RES_PARERR;
		break;
	}

	return res;
}
