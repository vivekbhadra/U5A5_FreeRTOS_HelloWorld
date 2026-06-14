/* Core/Src/fdcan_updater.c */
#include "fdcan_updater.h"
#include <stdio.h>
#include <string.h>       /* Added for memcpy */
#include "FreeRTOS.h"     /* Added for FreeRTOS base configuration */
#include "task.h"         /* Added for pcTaskGetName */
  
/* Access the CubeMX generated FDCAN handle */
extern FDCAN_HandleTypeDef hfdcan1;
  
/**
* @brief Configures standard FDCAN filters to routing update range into FIFO 0,
*        and starts the CAN controller.
*/
HAL_StatusTypeDef App_FDCAN_Configure_Filters(void) {
  FDCAN_FilterTypeDef sFilterConfig;
  
  printf("[FDCAN] Configuring hardware message filters...\r\n");
  
  /* Filter 0: Route 0x200 to 0x201 (Init & Segment) to FIFO 0 */
  sFilterConfig.IdType = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex = 0;
  sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = CAN_ID_FW_INIT;    /* 0x200 */
  sFilterConfig.FilterID2 = CAN_ID_FW_SEGMENT; /* 0x201 */
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK) {
    printf("[FDCAN] ERROR: Failed to configure filter 0\r\n");
    return HAL_ERROR;
  }
  
  /* Filter 1: Route 0x202 (Segment Request) to FIFO 1 */
  sFilterConfig.FilterIndex = 1;
  sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO1;
  sFilterConfig.FilterID1 = CAN_ID_FW_SEG_REQ; /* 0x202 */
  sFilterConfig.FilterID2 = CAN_ID_FW_SEG_REQ; /* 0x202 */
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK) {
    printf("[FDCAN] ERROR: Failed to configure filter 1\r\n");
    return HAL_ERROR;
  }
  
  /* Filter 2: Route 0x203 (Checksum) to FIFO 0 */
  sFilterConfig.FilterIndex = 2;
  sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1 = CAN_ID_FW_CHECKSUM; /* 0x203 */
  sFilterConfig.FilterID2 = CAN_ID_FW_CHECKSUM; /* 0x203 */
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK) {
    printf("[FDCAN] ERROR: Failed to configure filter 2\r\n");
    return HAL_ERROR;
  }
  
  /* Filter 3: Route 0x204 to 0x205 (Checksum Request & End) to FIFO 1 */
  sFilterConfig.FilterIndex = 3;
  sFilterConfig.FilterType = FDCAN_FILTER_RANGE;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO1;
  sFilterConfig.FilterID1 = CAN_ID_FW_CHKSUM_REQ; /* 0x204 */
  sFilterConfig.FilterID2 = CAN_ID_FW_END;        /* 0x205 */
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK) {
    printf("[FDCAN] ERROR: Failed to configure filter 3\r\n");
    return HAL_ERROR;
  }
  
  /* Configure global filters to reject anything outside our filter ranges */
  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_REJECT_REMOTE,
                                   FDCAN_REJECT_REMOTE) != HAL_OK) {
    printf("[FDCAN] ERROR: Failed to configure Global rejection filters!\r\n");
    return HAL_ERROR;
  }
  
  /* Start the FDCAN controller */
  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
    printf("[FDCAN] ERROR: FDCAN failed to transition to START state!\r\n");
    return HAL_ERROR;
  }
  
  /* Flush any stale frames left in the FIFOs from a previous run/boot
     before we enable the interrupt that feeds the queue. */
  FDCAN_RxHeaderTypeDef tmpHdr;
  uint8_t tmpData[64];
  while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0) {
    HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &tmpHdr, tmpData);
  }
  while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO1) > 0) {
    HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO1, &tmpHdr, tmpData);
  }
  
  /* Enable FIFO 0 Interrupt (Used by Slave) */
  HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
  
  HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
  
  printf("[FDCAN] Filters configured. Hardware is ACTIVE and listening.\r\n");
  return HAL_OK;
}

/**
* @brief Transmits a message over FDCAN with comprehensive hardware and
* loopback diagnostic traces.
*/
HAL_StatusTypeDef
App_FDCAN_Send_Message(uint32_t can_id, const uint8_t *payload, uint16_t size) {
  FDCAN_TxHeaderTypeDef TxHeader = {0};
  uint8_t tx_payload[64] = {0};
  uint16_t mapped_bytes = 0;
  
  if (size > 64U) {
    printf("[FDCAN TX] ERROR: Payload too large: %u bytes\r\n", (unsigned int)size);
    return HAL_ERROR;
  }
  
  memcpy(tx_payload, payload, size);
  
  /* Task name lookup is now safe under RTOS since task.h is included */
  printf("[FDCAN TX] Task=%s, ID=0x%03lX, Size=%u bytes\r\n",
         pcTaskGetName(NULL), (unsigned long)can_id, (unsigned int)size);
  
  printf("[FDCAN TX] >>> Sending Msg: ID=0x%03lX, Size=%u bytes\r\n",
         (unsigned long)can_id, (unsigned int)size);
  
  /* Configure standard FDCAN TX Header attributes */
  TxHeader.Identifier = can_id;
  TxHeader.IdType = FDCAN_STANDARD_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  TxHeader.FDFormat = FDCAN_FD_CAN;      /* Select CAN FD Mode */
  TxHeader.BitRateSwitch = FDCAN_BRS_ON; /* Enable Baud rate acceleration */
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;
  
  switch (size) {
    case 0U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_0;
      mapped_bytes = 0U;
      break;
    case 1U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_1;
      mapped_bytes = 1U;
      break;
    case 2U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_2;
      mapped_bytes = 2U;
      break;
    case 3U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_3;
      mapped_bytes = 3U;
      break;
    case 4U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_4;
      mapped_bytes = 4U;
      break;
    case 5U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_5;
      mapped_bytes = 5U;
      break;
    case 6U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_6;
      mapped_bytes = 6U;
      break;
    case 7U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_7;
      mapped_bytes = 7U;
      break;
    case 8U:
      TxHeader.DataLength = FDCAN_DLC_BYTES_8;
      mapped_bytes = 8U;
      break;
  
    default:
      if (size <= 12U) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_12;
        mapped_bytes = 12U;
      } else if (size <= 16U) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_16;
        mapped_bytes = 16U;
      } else if (size <= 20U) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_20;
        mapped_bytes = 20U;
      } else if (size <= 24U) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_24;
        mapped_bytes = 24U;
      } else if (size <= 32U) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_32;
        mapped_bytes = 32U;
      } else if (size <= 48U) {
        TxHeader.DataLength = FDCAN_DLC_BYTES_48;
        mapped_bytes = 48U;
      } else {
        TxHeader.DataLength = FDCAN_DLC_BYTES_64;
        mapped_bytes = 64U;
      }
      break;
  }
  
  printf("[FDCAN TX] DLC mapped: requested=%u, mapped=%u, DataLength=0x%08lX\r\n",
         (unsigned int)size, (unsigned int)mapped_bytes,
         (unsigned long)TxHeader.DataLength);
  
  /* 1. Queue the message inside the hardware Tx FIFO */
  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, tx_payload) != HAL_OK) {
    printf("[FDCAN TX] ERROR: Hardware refused to queue ID 0x%03lX!\r\n", (unsigned long)can_id);
    return HAL_ERROR;
  }
  
  /* 2. Determine which Tx buffer index this message was allocated to */
  uint32_t latest_tx_buffer = HAL_FDCAN_GetLatestTxFifoQRequestBuffer(&hfdcan1);
  
  /* 3. Wait briefly for the physical transmission attempt to complete */
  for (volatile int delay = 0; delay < 1500; delay++) {
    __NOP();
  }
  
  /* 4. Query Hardware Status Registers to evaluate the transmission outcome */
  FDCAN_ProtocolStatusTypeDef protocolStatus = {0};
  FDCAN_ErrorCountersTypeDef errorCounters = {0};
  
  HAL_FDCAN_GetProtocolStatus(&hfdcan1, &protocolStatus);
  HAL_FDCAN_GetErrorCounters(&hfdcan1, &errorCounters);
  uint32_t is_pending = HAL_FDCAN_IsTxBufferMessagePending(&hfdcan1, latest_tx_buffer);
  
  /* 4b. Read the raw peripheral registers directly */
  uint32_t raw_psr = hfdcan1.Instance->PSR;     /* Protocol Status Register   */
  uint32_t raw_rxf0s = hfdcan1.Instance->RXF0S; /* Rx FIFO 0 Status Register  */
  uint32_t raw_rxf1s = hfdcan1.Instance->RXF1S; /* Rx FIFO 1 Status Register  */
  uint32_t raw_ir = hfdcan1.Instance->IR;       /* Interrupt (event) Register */
  
  uint32_t rxf0_fill = raw_rxf0s & 0x7U;
  uint32_t rxf1_fill = raw_rxf1s & 0x7U;
  
  const char *activity_str;
  switch (protocolStatus.Activity) {
    case FDCAN_COM_STATE_SYNC:
      activity_str = "SYNCHRONIZING (not yet locked)";
      break;
    case FDCAN_COM_STATE_IDLE:
      activity_str = "IDLE (synced, no traffic)";
      break;
    case FDCAN_COM_STATE_RX:
      activity_str = "RECEIVER";
      break;
    case FDCAN_COM_STATE_TX:
      activity_str = "TRANSMITTER";
      break;
    default:
      activity_str = "UNKNOWN";
      break;
  }
  
  /* 5. Print Detailed Trace Metrics */
  printf("[FDCAN DIAG] Msg ID=0x%03lX assigned to TxBuffer bitmask: 0x%08lX\r\n", 
         (unsigned long)can_id, (unsigned long)latest_tx_buffer);
  printf("[FDCAN DIAG] Tx Buffer Pending: %s\r\n", is_pending ? "YES (still queued)" : "NO (left the Tx buffer)");
  printf("[FDCAN DIAG] Protocol Activity: %s\r\n", activity_str);
  printf("[FDCAN DIAG] LEC=0x%lX  (0x0=NoError 0x3=ACK 0x7=NoChange)\r\n", (unsigned long)protocolStatus.LastErrorCode);
  printf("[FDCAN DIAG] Hardware Error Counters: TEC=%lu, REC=%lu\r\n",
         (unsigned long)errorCounters.TxErrorCnt, (unsigned long)errorCounters.RxErrorCnt);
  printf("[FDCAN DIAG] RAW PSR=0x%08lX  IR=0x%08lX\r\n", (unsigned long)raw_psr, (unsigned long)raw_ir);
  printf("[FDCAN DIAG] RAW RXF0S=0x%08lX (fill=%lu)  RXF1S=0x%08lX (fill=%lu)\r\n",
         (unsigned long)raw_rxf0s, (unsigned long)rxf0_fill,
         (unsigned long)raw_rxf1s, (unsigned long)rxf1_fill);
  printf("[FDCAN DIAG] >> READ THIS: if both fills=0 the looped frame was DROPPED.\r\n");
  printf("[FDCAN DIAG] >>   Activity=SYNCHRONIZING + fills=0  => bit-timing problem.\r\n");
  printf("[FDCAN DIAG] >>   Activity=IDLE/TX     + fills=0  => filter/routing problem.\r\n");
  printf("-----------------------------------------------------------------\r\n\r\n");
  
  return HAL_OK;
}
