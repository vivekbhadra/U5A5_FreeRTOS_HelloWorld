/* Core/Inc/fdcan_updater.h */
#ifndef FDCAN_UPDATER_H
#define FDCAN_UPDATER_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

/* CAN message IDs matching Rust host framework */
#define CAN_ID_FW_INIT          0x200U
#define CAN_ID_FW_SEGMENT       0x201U
#define CAN_ID_FW_SEG_REQ       0x202U
#define CAN_ID_FW_CHECKSUM      0x203U
#define CAN_ID_FW_CHKSUM_REQ    0x204U
#define CAN_ID_FW_END           0x205U

/* Firmware Update Abort/End Reasons matching the Host */
typedef enum {
    REJECT_SUCCEEDED = 0,
    REJECT_UPDATE_IN_PROGRESS,
    REJECT_PROTOCOL_ERROR,
    REJECT_FIRMWARE_LENGTH_INVALID,
    REJECT_TIMEOUT,
    REJECT_CRC_ERROR,
    REJECT_NOR_FLASH_ERROR_OTHER,
    REJECT_NO_UPDATE_IN_PROGRESS
} FirmwareUpdateEndReason;

/* 1. FirmwareUpdateInit (8 Bytes) */
typedef struct __attribute__((packed)) {
    uint16_t update_id;
    uint16_t recipient_id;
    uint32_t total_size;
} FirmwareUpdateInit_t;

/* 2. FirmwareUpdateSegment (39 Bytes) */
typedef struct __attribute__((packed)) {
    uint16_t update_id;
    uint32_t segment_id;
    uint8_t  data_length;      /* Max 32 */
    uint8_t  segment_data[32];
} FirmwareUpdateSegment_t;

/* 3. FirmwareUpdateSegmentRequest (6 Bytes) */
typedef struct __attribute__((packed)) {
    uint16_t update_id;
    uint32_t segment_id;
} FirmwareUpdateSegmentRequest_t;

/* 4. FirmwareUpdateChecksumRequest (2 Bytes) */
typedef struct __attribute__((packed)) {
    uint16_t update_id;
} FirmwareUpdateChecksumRequest_t;

/* 5. FirmwareUpdateChecksum (6 Bytes) */
typedef struct __attribute__((packed)) {
    uint16_t update_id;
    uint32_t crc;
} FirmwareUpdateChecksum_t;

/* 6. FirmwareUpdateEnd (3 Bytes) */
typedef struct __attribute__((packed)) {
    uint16_t update_id;
    uint8_t  reject_reason;    /* Maps to FirmwareUpdateEndReason */
} FirmwareUpdateEnd_t;

/* Structure representing an internal FreeRTOS queue frame */
typedef struct {
    uint32_t can_id;
    uint16_t data_length;
    uint8_t  payload[64];      /* Fits max FD frame */
} FwCanRxQueueMsg_t;

/* Initialization Prototypes */
HAL_StatusTypeDef App_FDCAN_Configure_Filters(void);
HAL_StatusTypeDef App_FDCAN_Send_Message(uint32_t can_id, const uint8_t *payload, uint16_t size);

#endif /* FDCAN_UPDATER_H */