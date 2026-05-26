/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    sd_diskio.c
  * @brief   SD Disk I/O driver
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Note: code generation based on sd_diskio_dma_template_bspv1.c v2.1.4
   as "Use dma template" is enabled. */

/* USER CODE BEGIN firstSection */
/* can be used to modify / undefine following code or add new definitions */
/* USER CODE END firstSection*/

/* Includes ------------------------------------------------------------------*/
#include "ff_gen_drv.h"
#include "sd_diskio.h"

#include <string.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* 将超时收敛到 1 秒，避免启动阶段长时间假死 */
#define SD_TIMEOUT 1000

#define SD_DEFAULT_BLOCK_SIZE 512

/*
 * Depending on the use case, the SD card initialization could be done at the
 * application level: if it is the case define the flag below to disable
 * the BSP_SD_Init() call in the SD_Initialize() and add a call to
 * BSP_SD_Init() elsewhere in the application.
 */
/* USER CODE BEGIN disableSDInit */
/* #define DISABLE_SD_INIT */
/* USER CODE END disableSDInit */

/*
 * when using cacheable memory region, it may be needed to maintain the cache
 * validity. Enable the define below to activate a cache maintenance at each
 * read and write operation.
 * Notice: This is applicable only for cortex M7 based platform.
 */
/* USER CODE BEGIN enableSDDmaCacheMaintenance */
/* #define ENABLE_SD_DMA_CACHE_MAINTENANCE  1 */
/* USER CODE END enableSDDmaCacheMaintenance */

/*
* Some DMA requires 4-Byte aligned address buffer to correctly read/write data,
* in FatFs some accesses aren't thus we need a 4-byte aligned scratch buffer to correctly
* transfer data
*/
/* USER CODE BEGIN enableScratchBuffer */
#define ENABLE_SCRATCH_BUFFER
/* USER CODE END enableScratchBuffer */

/* Private variables ---------------------------------------------------------*/
#if defined(ENABLE_SCRATCH_BUFFER)
#if defined (ENABLE_SD_DMA_CACHE_MAINTENANCE)
ALIGN_32BYTES(static uint8_t scratch[BLOCKSIZE]);
#else
__ALIGN_BEGIN static uint8_t scratch[BLOCKSIZE] __ALIGN_END;
#endif
#endif

static volatile DSTATUS Stat = STA_NOINIT;
static volatile UINT WriteStatus = 0, ReadStatus = 0;

/* Private function prototypes -----------------------------------------------*/
static DSTATUS SD_CheckStatus(BYTE lun);
DSTATUS SD_initialize(BYTE);
DSTATUS SD_status(BYTE);
DRESULT SD_read(BYTE, BYTE*, DWORD, UINT);
#if _USE_WRITE == 1
DRESULT SD_write(BYTE, const BYTE*, DWORD, UINT);
#endif
#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE, BYTE, void*);
#endif

const Diskio_drvTypeDef SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
#if _USE_WRITE == 1
  SD_write,
#endif
#if _USE_IOCTL == 1
  SD_ioctl,
#endif
};

static int SD_CheckStatusWithTimeout(uint32_t timeout)
{
  uint32_t timer = HAL_GetTick();

  while ((HAL_GetTick() - timer) < timeout)
  {
    if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
    {
      return 0;
    }
  }

  return -1;
}

static DSTATUS SD_CheckStatus(BYTE lun)
{
  (void)lun;
  Stat = STA_NOINIT;

  if (BSP_SD_GetCardState() == MSD_OK)
  {
    Stat &= ~STA_NOINIT;
  }

  return Stat;
}

DSTATUS SD_initialize(BYTE lun)
{
#if !defined(DISABLE_SD_INIT)
  if (BSP_SD_Init() == MSD_OK)
  {
    Stat = SD_CheckStatus(lun);
  }
#else
  Stat = SD_CheckStatus(lun);
#endif

  return Stat;
}

DSTATUS SD_status(BYTE lun)
{
  return SD_CheckStatus(lun);
}

DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;
#if defined(ENABLE_SCRATCH_BUFFER)
  uint8_t ret;
#endif

  (void)lun;

  if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0)
  {
    return res;
  }

#if defined(ENABLE_SCRATCH_BUFFER)
  if (!((uint32_t)buff & 0x3U))
  {
#endif
    if (BSP_SD_ReadBlocks((uint32_t*)buff, (uint32_t)sector, count, SD_TIMEOUT) == MSD_OK)
    {
      if (SD_CheckStatusWithTimeout(SD_TIMEOUT) == 0)
      {
        res = RES_OK;
      }
    }
#if defined(ENABLE_SCRATCH_BUFFER)
  }
  else
  {
    int i;

    for (i = 0; i < (int)count; i++)
    {
      ret = BSP_SD_ReadBlocks((uint32_t*)scratch, (uint32_t)sector++, 1, SD_TIMEOUT);
      if (ret == MSD_OK)
      {
        if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0)
        {
          res = RES_ERROR;
          break;
        }

        memcpy(buff, scratch, BLOCKSIZE);
        buff += BLOCKSIZE;
      }
      else
      {
        break;
      }
    }

    if ((i == (int)count) && (ret == MSD_OK))
    {
      res = RES_OK;
    }
  }
#endif

  return res;
}

#if _USE_WRITE == 1
DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  DRESULT res = RES_ERROR;
#if defined(ENABLE_SCRATCH_BUFFER)
  uint8_t ret;
  int i;
#endif

  (void)lun;

  if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0)
  {
    return res;
  }

#if defined(ENABLE_SCRATCH_BUFFER)
  if (!((uint32_t)buff & 0x3U))
  {
#endif
    if (BSP_SD_WriteBlocks((uint32_t*)buff, (uint32_t)sector, count, SD_TIMEOUT) == MSD_OK)
    {
      if (SD_CheckStatusWithTimeout(SD_TIMEOUT) == 0)
      {
        res = RES_OK;
      }
    }
#if defined(ENABLE_SCRATCH_BUFFER)
  }
  else
  {
    for (i = 0; i < (int)count; i++)
    {
      memcpy((void*)scratch, (const void*)buff, BLOCKSIZE);
      buff += BLOCKSIZE;

      ret = BSP_SD_WriteBlocks((uint32_t*)scratch, (uint32_t)sector++, 1, SD_TIMEOUT);
      if (ret == MSD_OK)
      {
        if (SD_CheckStatusWithTimeout(SD_TIMEOUT) < 0)
        {
          break;
        }
      }
      else
      {
        break;
      }
    }

    if ((i == (int)count) && (ret == MSD_OK))
    {
      res = RES_OK;
    }
  }
#endif

  return res;
}
#endif

#if _USE_IOCTL == 1
DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  BSP_SD_CardInfo CardInfo;

  (void)lun;

  if (Stat & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  switch (cmd)
  {
    case CTRL_SYNC:
      res = RES_OK;
      break;

    case GET_SECTOR_COUNT:
      BSP_SD_GetCardInfo(&CardInfo);
      *(DWORD*)buff = CardInfo.LogBlockNbr;
      res = RES_OK;
      break;

    case GET_SECTOR_SIZE:
      BSP_SD_GetCardInfo(&CardInfo);
      *(WORD*)buff = CardInfo.LogBlockSize;
      res = RES_OK;
      break;

    case GET_BLOCK_SIZE:
      BSP_SD_GetCardInfo(&CardInfo);
      *(DWORD*)buff = CardInfo.LogBlockSize / SD_DEFAULT_BLOCK_SIZE;
      res = RES_OK;
      break;

    default:
      res = RES_PARERR;
      break;
  }

  return res;
}
#endif

void BSP_SD_WriteCpltCallback(void)
{
  WriteStatus = 1;
}

void BSP_SD_ReadCpltCallback(void)
{
  ReadStatus = 1;
}
