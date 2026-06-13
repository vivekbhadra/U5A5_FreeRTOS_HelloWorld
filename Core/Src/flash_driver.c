/* Core/Src/flash_driver.c */
#include "flash_driver.h"
#include <string.h>
#include <stdio.h> // Added for print diagnostics

const uint8_t MAGIC_BOOT[16] = {0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0,0xD0};
const uint8_t MAGIC_SWAP[16] = {0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1,0xD1};

/**
 * @brief Erases pages in Bank 1 with safe ICACHE handling.
 */
HAL_StatusTypeDef App_Flash_Erase(uint32_t start_address, uint32_t size) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t PageError = 0;
    HAL_StatusTypeDef status;

    printf("[FLASH] Erase requested: Addr=0x%08lX, Size=%lu bytes\r\n", 
           (unsigned long)start_address, (unsigned long)size);

    /* 1. Disable the instruction cache before modifying flash memory */
    if (HAL_ICACHE_Disable() != HAL_OK) {
        printf("[FLASH] ERROR: Failed to disable ICACHE!\r\n");
        return HAL_ERROR;
    }

    HAL_FLASH_Unlock();

    /* STM32U5 page address offset calculations */
    uint32_t start_page = (start_address - 0x08000000U) / FLASH_PAGE_SIZE;
    uint32_t nb_pages = (size + (FLASH_PAGE_SIZE - 1)) / FLASH_PAGE_SIZE;

    printf("[FLASH] Erasing Bank 1: Start Page=%lu, Count=%lu...\r\n", 
           (unsigned long)start_page, (unsigned long)nb_pages);

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Banks     = FLASH_BANK_1;
    EraseInitStruct.Page      = start_page;
    EraseInitStruct.NbPages   = nb_pages;

    status = HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);

    HAL_FLASH_Lock();

    /* 2. Re-enable instruction cache and invalidate stale content */
    HAL_ICACHE_Enable();

    if (status == HAL_OK) {
        printf("[FLASH] Erase completed successfully.\r\n");
    } else {
        printf("[FLASH] ERROR: Erase failed! Status=%d, PageError=%lu\r\n", 
               status, (unsigned long)PageError);
    }

    return status;
}

/**
 * @brief Writes quad-word aligned (16-byte) data blocks to STM32U5 Flash.
 */
HAL_StatusTypeDef App_Flash_Write(uint32_t start_address, const uint8_t *data, uint32_t length) {
    HAL_StatusTypeDef status = HAL_OK;

    printf("[FLASH] Write requested: Addr=0x%08lX, Length=%lu bytes\r\n", 
           (unsigned long)start_address, (unsigned long)length);

    /* Address must be strictly aligned to 16-byte physical boundaries */
    if ((start_address % 16) != 0) {
        printf("[FLASH] ERROR: Write address is not 16-byte aligned!\r\n");
        return HAL_ERROR;
    }

    if (HAL_ICACHE_Disable() != HAL_OK) {
        printf("[FLASH] ERROR: Failed to disable ICACHE!\r\n");
        return HAL_ERROR;
    }

    HAL_FLASH_Unlock();

    uint32_t num_quadwords = (length + 15) / 16;
    uint8_t temp_buf[16];

    printf("[FLASH] Programming %lu quad-words...\r\n", (unsigned long)num_quadwords);

    for (uint32_t i = 0; i < num_quadwords; i++) {
        uint32_t current_addr = start_address + (i * 16);
        const uint8_t *src_ptr = &data[i * 16];
        uint32_t remaining = length - (i * 16);

        /* Pad short blocks with 0xFF to preserve quad-word alignment */
        if (remaining < 16) {
            memset(temp_buf, 0xFF, 16);
            memcpy(temp_buf, src_ptr, remaining);
            src_ptr = temp_buf;
        }

        /* Program 16-byte block from source address */
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, current_addr, (uint32_t)src_ptr);
        if (status != HAL_OK) {
            printf("[FLASH] ERROR: Program failed at Addr=0x%08lX, Status=%d\r\n", 
                   (unsigned long)current_addr, status);
            break;
        }
    }

    HAL_FLASH_Lock();
    HAL_ICACHE_Enable();

    if (status == HAL_OK) {
        printf("[FLASH] Write completed successfully.\r\n");
    }

    return status;
}