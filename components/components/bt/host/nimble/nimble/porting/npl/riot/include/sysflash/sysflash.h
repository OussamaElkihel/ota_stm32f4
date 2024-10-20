/**
 * This file was generated by Apache newt version: 1.11.0-dev
 */

#ifndef H_MYNEWT_SYSFLASH_
#define H_MYNEWT_SYSFLASH_

#include "flash_map/flash_map.h"

/**
 * This flash map definition is used for two purposes:
 * 1. To locate the meta area, which contains the true flash map definition.
 * 2. As a fallback in case the meta area cannot be read from flash.
 */
extern const struct flash_area sysflash_map_dflt[6];

#define FLASH_AREA_BOOTLOADER                    0
#define FLASH_AREA_BOOTLOADER_DEVICE             0
#define FLASH_AREA_BOOTLOADER_OFFSET             0x00000000
#define FLASH_AREA_BOOTLOADER_SIZE               32768

#define FLASH_AREA_IMAGE_0                       1
#define FLASH_AREA_IMAGE_0_DEVICE                0
#define FLASH_AREA_IMAGE_0_OFFSET                0x0000c000
#define FLASH_AREA_IMAGE_0_SIZE                  483328

#define FLASH_AREA_IMAGE_1                       2
#define FLASH_AREA_IMAGE_1_DEVICE                0
#define FLASH_AREA_IMAGE_1_OFFSET                0x00082000
#define FLASH_AREA_IMAGE_1_SIZE                  483328

#define FLASH_AREA_IMAGE_SCRATCH                 3
#define FLASH_AREA_IMAGE_SCRATCH_DEVICE          0
#define FLASH_AREA_IMAGE_SCRATCH_OFFSET          0x000f8000
#define FLASH_AREA_IMAGE_SCRATCH_SIZE            16384

#define FLASH_AREA_REBOOT_LOG                    16
#define FLASH_AREA_REBOOT_LOG_DEVICE             0
#define FLASH_AREA_REBOOT_LOG_OFFSET             0x00008000
#define FLASH_AREA_REBOOT_LOG_SIZE               16384

#define FLASH_AREA_NFFS                          17
#define FLASH_AREA_NFFS_DEVICE                   0
#define FLASH_AREA_NFFS_OFFSET                   0x000fc000
#define FLASH_AREA_NFFS_SIZE                     16384

#endif
