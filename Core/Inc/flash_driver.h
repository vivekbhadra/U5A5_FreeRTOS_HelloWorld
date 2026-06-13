#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include "stm32u5xx_hal.h"

/* Partition Layout Map */
#define ADDR_BOOT_STATE        0x08020000U
#define ADDR_FLASH_ACTIVE      0x08040000U
#define ADDR_FLASH_DFU         0x08100000U

//#define FLASH_PAGE_SIZE        8192U         /* 8 KB page granularity */
#define DFU_PARTITION_SIZE     (768 * 1024)  /* 768 KB */

/* Magic status flags (16-byte aligned array) */
extern const uint8_t MAGIC_BOOT[16];
extern const uint8_t MAGIC_SWAP[16];
extern const uint8_t MAGIC_REVERT[16];

/* Driver Functions */
HAL_StatusTypeDef App_Flash_Erase(uint32_t start_address, uint32_t size);
HAL_StatusTypeDef App_Flash_Write(uint32_t start_address, const uint8_t *data, uint32_t length);
/* Add to Core/Inc/flash_driver.h */
void Mark_Firmware_As_Valid(void);

#endif /* FLASH_DRIVER_H */
