/* Core/Src/fdcan_updater.c */
#include "fdcan_updater.h"
#include <stdio.h>

/* Access the CubeMX generated FDCAN handle */
extern FDCAN_HandleTypeDef hfdcan1;

/**
 * @brief Configures standard FDCAN filters to routing update range into FIFO 0, 
 *        and starts the CAN controller.
 */
HAL_StatusTypeDef App_FDCAN_Configure_Filters() {
    FDCAN_FilterTypeDef sFilterConfig;

    /* 1. NVIC Interrupt Configuration (Moved from main to keep main clean) */
    // HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 5, 0); 
    // HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);         
    // printf("[FDCAN] Configuring hardware message filters...\r\n");

    /* 1. Setup range filter to accept Standard IDs 0x200 to 0x205 */
    sFilterConfig.IdType = FDCAN_STANDARD_ID;
    sFilterConfig.FilterIndex = 0;
    sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
    sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    sFilterConfig.FilterID1 = CAN_ID_FW_INIT;      /* 0x200 */
    sFilterConfig.FilterID2 = CAN_ID_FW_END;       /* 0x205 */

    if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK) {
        printf("[FDCAN] ERROR: Failed to configure range filters!\r\n");
        return HAL_ERROR;
    } else {
        printf("[FDCAN]: Configured range filters!\r\n");
    }

    /* 2. Configure global filters to reject anything outside our update ID ranges */
    if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, 
                                     FDCAN_REJECT, 
                                     FDCAN_REJECT, 
                                     FDCAN_REJECT_REMOTE, 
                                     FDCAN_REJECT_REMOTE) != HAL_OK) {
        printf("[FDCAN] ERROR: Failed to configure Global rejection filters!\r\n");
        return HAL_ERROR;
    } else {
        printf("[FDCAN]: Configured Global rejection filters!\r\n");
    }

    /* 3. Start the FDCAN controller */
    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        printf("[FDCAN] ERROR: FDCAN failed to transition to START state!\r\n");
        return HAL_ERROR;
    } else {
        printf("[FDCAN]: successfully transitioned to START state!\r\n");
    }

    /* 4. Enable FIFO 0 Interrupt (So we can queue messages inside our ISR callback) */
    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        printf("[FDCAN] ERROR: Failed to activate FIFO0 RX interrupt!\r\n");
        return HAL_ERROR;
    } else {
        printf("[FDCAN]: successfully activated FIFO0 RX interrupt!\r\n");
    }

    printf("[FDCAN] Filters configured. Hardware is ACTIVE and listening.\r\n");
    return HAL_OK;
}

/**
 * @brief Transmits a message over FDCAN with correct CAN FD formatting
 */
HAL_StatusTypeDef App_FDCAN_Send_Message(uint32_t can_id, const uint8_t *payload, uint16_t size) {
    FDCAN_TxHeaderTypeDef TxHeader;
    uint16_t mapped_bytes = 0;

    /* STEP 1: Log Function Entry and Raw Input Parameters */
    printf("[FDCAN TX] >>> Sending Msg: ID=0x%03lX, Raw Payload Size=%u bytes\r\n", 
           (unsigned long)can_id, (unsigned int)size);

    /* Configure standard FDCAN TX Header attributes */
    TxHeader.Identifier = can_id;
    TxHeader.IdType = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    TxHeader.FDFormat = FDCAN_FD_CAN;             /* Select CAN FD Mode */
    TxHeader.BitRateSwitch = FDCAN_BRS_ON;        /* Enable Baud rate acceleration */
    TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker = 0;

    /* STEP 2: Map arbitrary size to strict non-linear CAN FD DLC hardware enums */
    if (size <= 8) {
        TxHeader.DataLength = size;
        mapped_bytes = size;
    } else if (size <= 12) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_12;
        mapped_bytes = 12;
    } else if (size <= 16) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_16;
        mapped_bytes = 16;
    } else if (size <= 20) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_20;
        mapped_bytes = 20;
    } else if (size <= 24) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_24;
        mapped_bytes = 24;
    } else if (size <= 32) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_32;
        mapped_bytes = 32;
    } else if (size <= 48) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_48;
        mapped_bytes = 48;
    } else {
        TxHeader.DataLength = FDCAN_DLC_BYTES_64;
        mapped_bytes = 64;
    }

    /* Log the DLC mapping result */
    printf("[FDCAN TX] Mapping DLC: requested=%u bytes -> mapped to hardware limit=%u bytes\r\n", 
           (unsigned int)size, (unsigned int)mapped_bytes);

    /* STEP 3: Print a Hexadecimal Preview of the Payload */
    if (payload != NULL && mapped_bytes > 0) {
        uint16_t preview_len = (mapped_bytes > 8) ? 8 : mapped_bytes;
        printf("[FDCAN TX] Payload Hex Preview (first %u bytes): ", (unsigned int)preview_len);
        for (int i = 0; i < preview_len; i++) {
            printf("%02X ", payload[i]);
        }
        if (mapped_bytes > 8) {
            printf("..."); /* Indicate there are more bytes in the packet */
        }
        printf("\r\n");
    } else {
        printf("[FDCAN TX] Payload is NULL or empty.\r\n");
    }

    /* STEP 4: Log Hardware Queue Action and Evaluate FIFO Result */
    printf("[FDCAN TX] Attempting to write frame to STM32 TX FIFO...\r\n");

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, (uint8_t *)payload) != HAL_OK) {
        printf("[FDCAN TX] ERROR: Hardware refused to queue ID 0x%03lX (FIFO full or Controller Offline)!\r\n", 
               (unsigned long)can_id);
        return HAL_ERROR;
    } else {
        /* STEP 5: Success Confirmation Log */
        printf("[FDCAN TX] SUCCESS: Message ID 0x%03lX queued successfully.\r\n\r\n", (unsigned long)can_id);
    }
    return HAL_OK;
}