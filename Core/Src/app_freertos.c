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
  
/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
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
  .stack_size = 512 * 4 // Allocates 2048 bytes of stack for printf safety
};

osThreadId_t blueTaskHandle;
const osThreadAttr_t blueTask_attributes = {
  .name = "blueTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
};

osThreadId_t redTaskHandle;
const osThreadAttr_t redTask_attributes = {
  .name = "redTask",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 512 * 4
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
  /* USER CODE END RTOS_QUEUES */

  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
  
  /* USER CODE BEGIN RTOS_THREADS */
  // Create the Green, Blue, and Red Task threads
  greenTaskHandle = osThreadNew(StartGreenTask, NULL, &greenTask_attributes);
  blueTaskHandle = osThreadNew(StartBlueTask, NULL, &blueTask_attributes);
  redTaskHandle = osThreadNew(StartRedTask, NULL, &redTask_attributes);
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
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
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
    printf("[GREEN TASK] Green LED Toggled! (Interval: 500ms)\r\n");
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
    printf("  [BLUE TASK] Blue LED Toggled! (Interval: 1000ms)\r\n");
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
    printf("    [RED TASK] Red LED Toggled! (Interval: 1500ms)\r\n");
    osMutexRelease(printMutexHandle); // Release lock

    osDelay(1500);
  }
}

/* USER CODE END Application */