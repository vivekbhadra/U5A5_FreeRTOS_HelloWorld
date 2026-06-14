/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS Applicative File with 3 Blinking Tasks
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
  
/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"
#include "flash_driver.h"
#include "fdcan_updater.h"
#include "queue.h"
  
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "main.h" // Gives access to BSP LEDs and UART handles
/* USER CODE END Includes */
  
/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
  
/* USER CODE END PTD */
  
/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
  
/* USER CODE END PD */
  
/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
  
/* USER CODE END PM */

/* Extern the hardware CRC handle configured in main.c */
extern CRC_HandleTypeDef hcrc;
extern QueueHandle_t CanRxQueue;
/* Slave State Trackers */
typedef enum {
    UPDATER_STATE_IDLE,
    UPDATER_STATE_RECEIVING
} UpdaterState_t;

  
/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
extern FDCAN_HandleTypeDef hfdcan1;
extern CRC_HandleTypeDef hcrc;
QueueHandle_t CanRxQueue = NULL;     
osThreadId_t  updaterTaskHandle = NULL;   
osThreadId_t  masterSimTaskHandle = NULL; 

/* Task Function Prototypes */
void FirmwareUpdater_Task(void *argument);
void FwSimMaster_Task(void *argument);
// Mutex to protect printf from concurrent write interleaving
osMutexId_t printMutexHandle;
const osMutexAttr_t printMutex_attributes = {
  .name = "printMutex"
};
/* USER CODE END Variables */

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};

/* USER CODE BEGIN Definitions */
// Definitions for our 3 new LED blinking threads
osThreadId_t greenTaskHandle;
const osThreadAttr_t greenTask_attributes = {
  .name = "greenTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4 // Allocates 2048 bytes of stack for printf safety
};

osThreadId_t blueTaskHandle;
const osThreadAttr_t blueTask_attributes = {
  .name = "blueTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};

osThreadId_t redTaskHandle;
const osThreadAttr_t redTask_attributes = {
  .name = "redTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 128 * 4
};
/* USER CODE END Definitions */
  
/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
// Task function prototypes
void StartGreenTask(void *argument);
void StartBlueTask(void *argument);
void StartRedTask(void *argument);
/* USER CODE END FunctionPrototypes */
  
/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* USER CODE END Init */
  /* USER CODE BEGIN RTOS_MUTEX */
  // Create our Print Mutex to prevent garbled console text
  printMutexHandle = osMutexNew(&printMutex_attributes);
  /* USER CODE END RTOS_MUTEX */
  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */
  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */
  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  CanRxQueue = xQueueCreate(10, sizeof(FwCanRxQueueMsg_t));
  if (CanRxQueue == NULL) {
      printf("[SYSTEM] ERROR: Failed to create FDCAN RX Queue!\r\n");
  } else {
      printf("[SYSTEM] FDCAN RX Queue initialized successfully.\r\n");
  }
  /* USER CODE END RTOS_QUEUES */
  
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  /* USER CODE BEGIN RTOS_THREADS */
  // Create the Green, Blue, and Red Task threads
  greenTaskHandle = osThreadNew(StartGreenTask, NULL, &greenTask_attributes);
  blueTaskHandle = osThreadNew(StartBlueTask, NULL, &blueTask_attributes);
  redTaskHandle = osThreadNew(StartRedTask, NULL, &redTask_attributes);

  /* Incremental Addition: Create FDCAN Loopback Update Slave & Master Tasks */
  static const osThreadAttr_t updaterTask_attributes = {
    .name = "FwUpdaterTask",
    .stack_size = 2048,
    .priority = (osPriority_t) osPriorityAboveNormal,
  };
  static const osThreadAttr_t masterSimTask_attributes = {
    .name = "FwMasterSimTask",
    .stack_size = 2048,
    .priority = (osPriority_t) osPriorityNormal,
  };
  updaterTaskHandle = osThreadNew(FirmwareUpdater_Task, NULL, &updaterTask_attributes);
  masterSimTaskHandle = osThreadNew(FwSimMaster_Task, NULL, &masterSimTask_attributes);
  /* USER CODE END RTOS_THREADS */
  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
* @brief Function implementing the defaultTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDefaultTask */
/* Inside your default FreeRTOS startup task: */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */

  /* 1. Immediately handshake with the bootloader upon startup */
  Mark_Firmware_As_Valid();

  /* 2. Configure and activate FDCAN hardware filters
        Pass 1: Run Internal Loopback Test (No transceivers needed)
        Pass 0: Run Normal Bus Mode (Connects to physical CAN transceivers) */
  if (App_FDCAN_Configure_Filters() == HAL_OK) { // <-- Set to 1 for loopback testing
      printf("[SYSTEM] FDCAN Loopback started successfully.\r\n");
  } else {
      printf("[SYSTEM] FATAL ERROR: Failed to start FDCAN!\r\n");
  }

  
  printf("\r\n=============================================\r\n");
  printf("      FreeRTOS Application Initialized       \r\n");
  printf("=============================================\r\n");
  printf("[TEST] Initiating internal Flash Driver test...\r\n");

  /* Test payload sequence */
  uint8_t test_write_data[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  
  printf("[TEST] Step 1: Erasing DFU target page at 0x%08lX...\r\n", (unsigned long)ADDR_FLASH_DFU);
  
  /* Erase a single page at the start of the DFU space */
  if (App_Flash_Erase(ADDR_FLASH_DFU, FLASH_PAGE_SIZE) == HAL_OK) {
      
      printf("[TEST] Step 2: Programming 16-byte verification pattern...\r\n");
      
      /* Write the 16 bytes */
      if (App_Flash_Write(ADDR_FLASH_DFU, test_write_data, 16) == HAL_OK) {
          
          printf("[TEST] Step 3: Verifying flash contents...\r\n");
          
          /* Read back directly to verify */
          const uint8_t *read_ptr = (const uint8_t *)ADDR_FLASH_DFU;
          
          printf("[TEST] Memory Read-Back: ");
          for (int i = 0; i < 16; i++) {
              printf("0x%02X ", read_ptr[i]);
          }
          printf("\r\n");

          if (memcmp(read_ptr, test_write_data, 16) == 0) {
              printf("[TEST] Success! Verification MATCHED expected pattern.\r\n");
              
              /* Visual Confirmation: Slow Green LED Toggle */
              for (int i = 0; i < 4; i++) {
                  HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
                  HAL_Delay(500);
              }
          } else {
              printf("[TEST] ERROR: Verification FAILED! Data corruption detected.\r\n");
          }
      } else {
          printf("[TEST] ERROR: Write transaction aborted.\r\n");
      }
  } else {
      printf("[TEST] ERROR: Erase transaction aborted.\r\n");
  }

  printf("\r\n[SYSTEM] Flash test sequence completed. Starting application runtime...\r\n");

  /* Regular app loop continues... */
  for(;;)
  {
    //HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
    vTaskDelay(pdMS_TO_TICKS(250)); 
  }
  /* USER CODE END 5 */
}
  
/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* ======================================================================= */
/* TASK 1: GREEN LED TASK                                                  */
/* Blinks LED_GREEN every 500ms and prints its status                      */
/* ======================================================================= */
void StartGreenTask(void *argument)
{
  for(;;)
  {
    BSP_LED_Toggle(LED_GREEN);

    // Acquire lock: Nobody else can use printf() until we release it
    osMutexAcquire(printMutexHandle, osWaitForever);
    //printf("[GREEN TASK] Green LED Toggled! (Interval: 500ms)\r\n");
    osMutexRelease(printMutexHandle); // Release lock

    osDelay(500);
  }
}

/* ======================================================================= */
/* TASK 2: BLUE LED TASK                                                   */
/* Blinks LED_BLUE every 1000ms and prints its status                     */
/* ======================================================================= */
void StartBlueTask(void *argument)
{
  for(;;)
  {
    BSP_LED_Toggle(LED_BLUE);

    // Acquire lock
    osMutexAcquire(printMutexHandle, osWaitForever);
    //printf("  [BLUE TASK] Blue LED Toggled! (Interval: 1000ms)\r\n");
    osMutexRelease(printMutexHandle); // Release lock

    osDelay(1000);
  }
}

/* ======================================================================= */
/* TASK 3: RED LED TASK                                                    */
/* Blinks LED_RED every 1500ms and prints its status                       */
/* ======================================================================= */
void StartRedTask(void *argument)
{
  for(;;)
  {
    BSP_LED_Toggle(LED_RED);

    // Acquire lock
    osMutexAcquire(printMutexHandle, osWaitForever);
   // printf("    [RED TASK] Red LED Toggled! (Interval: 1500ms)\r\n");
    osMutexRelease(printMutexHandle); // Release lock

    osDelay(1500);
  }
}

void FirmwareUpdater_Task(void *argument) {
    FwCanRxQueueMsg_t rxMsg;
    UpdaterState_t state = UPDATER_STATE_IDLE;
    
    /* Session trackers */
    uint16_t active_update_id = 0;
    uint32_t total_size = 0;
    uint32_t expected_segment = 0;
    uint32_t total_bytes_written = 0;
    
    printf("[SLAVE] Production Updater Task Started.\r\n");

    for (;;) {
        if (xQueueReceive(CanRxQueue, &rxMsg, portMAX_DELAY) == pdPASS) {
            
            /* ---- 1. Handle Update Init ---- */
            if (rxMsg.can_id == CAN_ID_FW_INIT && state == UPDATER_STATE_IDLE) {
                FirmwareUpdateInit_t *init = (FirmwareUpdateInit_t *)rxMsg.payload;
                
                if (init->recipient_id == 0xAA) {
                    printf("[SLAVE] Init Recv: UpdateID=%u, Size=%lu bytes\r\n", 
                           init->update_id, (unsigned long)init->total_size);
                    
                    if (App_Flash_Erase(ADDR_FLASH_DFU, DFU_PARTITION_SIZE) == HAL_OK) {
                        __HAL_CRC_DR_RESET(&hcrc);
                        
                        active_update_id = init->update_id;
                        total_size = init->total_size;
                        expected_segment = 0;
                        total_bytes_written = 0;
                        state = UPDATER_STATE_RECEIVING;

                        FirmwareUpdateSegmentRequest_t req = { .update_id = active_update_id, .segment_id = 0 };
                        App_FDCAN_Send_Message(CAN_ID_FW_SEG_REQ, (uint8_t *)&req, sizeof(req));
                    } else {
                        FirmwareUpdateEnd_t end = { .update_id = init->update_id, .reject_reason = REJECT_NOR_FLASH_ERROR_OTHER };
                        App_FDCAN_Send_Message(CAN_ID_FW_END, (uint8_t *)&end, sizeof(end));
                    }
                }
            }
            
            /* ---- 2. Handle Segment Transmission ---- */
            else if (rxMsg.can_id == CAN_ID_FW_SEGMENT && state == UPDATER_STATE_RECEIVING) {
                FirmwareUpdateSegment_t *seg = (FirmwareUpdateSegment_t *)rxMsg.payload;

                if (seg->update_id != active_update_id || seg->segment_id != expected_segment) {
                    printf("[SLAVE] ERROR: Protocol violation! Expected seg %lu, got %lu\r\n", 
                           (unsigned long)expected_segment, (unsigned long)seg->segment_id);
                    state = UPDATER_STATE_IDLE;
                    FirmwareUpdateEnd_t end = { .update_id = active_update_id, .reject_reason = REJECT_PROTOCOL_ERROR };
                    App_FDCAN_Send_Message(CAN_ID_FW_END, (uint8_t *)&end, sizeof(end));
                    continue;
                }

                uint8_t write_block[32] = {0};
                memcpy(write_block, seg->segment_data, seg->data_length);
                
                uint32_t offset = seg->segment_id * 32;
                if (App_Flash_Write(ADDR_FLASH_DFU + offset, write_block, 32) == HAL_OK) {
                    
                    /* FIXED: Length is 8 Words (32 bytes / 4) because InputDataFormat is WORDS */
                    HAL_CRC_Accumulate(&hcrc, (uint32_t *)write_block, 8);
                    
                    expected_segment++;
                    total_bytes_written += seg->data_length;
                    
                    if (total_bytes_written == total_size) {
                        FirmwareUpdateChecksumRequest_t chk_req = { .update_id = active_update_id };
                        App_FDCAN_Send_Message(CAN_ID_FW_CHKSUM_REQ, (uint8_t *)&chk_req, sizeof(chk_req));
                    } else {
                        FirmwareUpdateSegmentRequest_t req = { .update_id = active_update_id, .segment_id = expected_segment };
                        App_FDCAN_Send_Message(CAN_ID_FW_SEG_REQ, (uint8_t *)&req, sizeof(req));
                    }
                } else {
                    state = UPDATER_STATE_IDLE;
                    FirmwareUpdateEnd_t end = { .update_id = active_update_id, .reject_reason = REJECT_NOR_FLASH_ERROR_OTHER };
                    App_FDCAN_Send_Message(CAN_ID_FW_END, (uint8_t *)&end, sizeof(end));
                }
            }
            
            /* ---- 3. Handle Checksum Validation ---- */
            else if (rxMsg.can_id == CAN_ID_FW_CHECKSUM && state == UPDATER_STATE_RECEIVING) {
                FirmwareUpdateChecksum_t *checksum = (FirmwareUpdateChecksum_t *)rxMsg.payload;
                state = UPDATER_STATE_IDLE;

                if (checksum->update_id == active_update_id) {
                    
                    /* FIXED 1: Read the accumulated CRC from the Data Register (DR) first, then reset */
                    uint32_t computed_crc = hcrc.Instance->DR; 
                    __HAL_CRC_DR_RESET(&hcrc);                 

                    if (computed_crc == checksum->crc) {
                        printf("[SLAVE] SUCCESS! CRCs MATCH (0x%08lX).\r\n", (unsigned long)computed_crc);
                        
                        FirmwareUpdateEnd_t end = { .update_id = active_update_id, .reject_reason = REJECT_SUCCEEDED };
                        App_FDCAN_Send_Message(CAN_ID_FW_END, (uint8_t *)&end, sizeof(end));
                        
                        vTaskDelay(pdMS_TO_TICKS(100));

                        printf("[SLAVE] Setting MAGIC_SWAP at 0x%08lX and rebooting...\r\n", (unsigned long)ADDR_BOOT_STATE);
                        if (App_Flash_Erase(ADDR_BOOT_STATE, FLASH_PAGE_SIZE) == HAL_OK) {
                            if (App_Flash_Write(ADDR_BOOT_STATE, MAGIC_SWAP, 16) == HAL_OK) {
                                printf("[SLAVE] System reboot initiated...\r\n");
                                NVIC_SystemReset();
                            }
                        }
                    } else {
                        printf("[SLAVE] ERROR: CRC MISMATCH! Computed: 0x%08lX, Expected: 0x%08lX\r\n", 
                               (unsigned long)computed_crc, (unsigned long)checksum->crc);
                        FirmwareUpdateEnd_t end = { .update_id = active_update_id, .reject_reason = REJECT_CRC_ERROR };
                        App_FDCAN_Send_Message(CAN_ID_FW_END, (uint8_t *)&end, sizeof(end));
                    }
                }
            }
        }
    }
}

/* Append to Core/Src/app_freertos.c */

void FwSimMaster_Task(void *argument) {
    uint8_t mock_binary[64];
    for (int i = 0; i < 64; i++) {
        mock_binary[i] = i;
    }
    
    /* FIXED: Length is 16 Words (64 bytes / 4) because InputDataFormat is WORDS */
    uint32_t expected_crc = HAL_CRC_Calculate(&hcrc, (uint32_t *)mock_binary, 16); 
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    printf("\r\n=============================================\r\n");
    printf("   LAUNCHING SELF-CONTAINED FDCAN LOOPBACK TEST   \r\n");
    printf("=============================================\r\n");
    printf("[MASTER] Mock Binary Size: 64 bytes, Expected CRC: 0x%08lX\r\n", (unsigned long)expected_crc);

    /* Send Init packet onto the internal loopback bus */
    FirmwareUpdateInit_t init = { .update_id = 999, .recipient_id = 0xAA, .total_size = 64 };
    App_FDCAN_Send_Message(CAN_ID_FW_INIT, (uint8_t *)&init, sizeof(init));

    for (;;) {
        FDCAN_RxHeaderTypeDef RxHeader;
        uint8_t payload[64];
        
        /* FIXED 2: Master polls FIFO 1 to prevent conflict with Slave's FIFO 0 ISR callback */
        if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO1, &RxHeader, payload) == HAL_OK) {
            uint32_t id = RxHeader.Identifier;
            
            /* ---- A. Handle Segment Request ---- */
            if (id == CAN_ID_FW_SEG_REQ) {
                FirmwareUpdateSegmentRequest_t *req = (FirmwareUpdateSegmentRequest_t *)payload;
                uint32_t offset = req->segment_id * 32;
                
                printf("[MASTER] Received request for Segment index %lu (Offset %lu)\r\n", 
                       (unsigned long)req->segment_id, (unsigned long)offset);

                FirmwareUpdateSegment_t seg;
                seg.update_id = req->update_id;
                seg.segment_id = req->segment_id;
                seg.data_length = 32;
                memcpy(seg.segment_data, &mock_binary[offset], 32);

                vTaskDelay(pdMS_TO_TICKS(100));
                App_FDCAN_Send_Message(CAN_ID_FW_SEGMENT, (uint8_t *)&seg, sizeof(seg));
            }
            
            /* ---- B. Handle Checksum Request ---- */
            else if (id == CAN_ID_FW_CHKSUM_REQ) {
                FirmwareUpdateChecksumRequest_t *req = (FirmwareUpdateChecksumRequest_t *)payload;
                printf("[MASTER] Received Checksum Request. Transmitting expected CRC: 0x%08lX...\r\n", 
                       (unsigned long)expected_crc);

                FirmwareUpdateChecksum_t checksum = { .update_id = req->update_id, .crc = expected_crc };
                vTaskDelay(pdMS_TO_TICKS(100));
                App_FDCAN_Send_Message(CAN_ID_FW_CHECKSUM, (uint8_t *)&checksum, sizeof(checksum));
            }
            
            /* ---- C. Handle End Transaction ---- */
            else if (id == CAN_ID_FW_END) {
                FirmwareUpdateEnd_t *end = (FirmwareUpdateEnd_t *)payload;
                printf("[MASTER] Transaction closed by slave. Reject Reason / Success Code: %d\r\n", 
                       end->reject_reason);
                
                if (end->reject_reason == REJECT_SUCCEEDED) {
                    printf("\r\n=============================================\r\n");
                    printf("     LOOPBACK TEST COMPLETED SUCCESSFULLY!    \r\n");
                    printf("=============================================\r\n");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
/* USER CODE END Application */
