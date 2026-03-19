/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f4xx_hal.h"
#include "oldType.h"
#include <stdio.h>
#include "oled.h"
#include "bmp.h"
#include "INA226.h"
#include "delay.h"
#include "rfid.h"

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

/* USER CODE BEGIN PV */
INA226_Data_t ina_data;
extern SPI_HandleTypeDef hspi1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
    delay_init();
	
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

    OLED_Init();
    OLED_ShowString(0, 16, "HELLO WORLD!", 16, 1);
    INA226_IO_Init();

    INA226_Searching();





    if (INA226_Init() == INA226_OK)
    {
        printf("INA226 Init Success!\r\n");
    }
    else
    {
        printf("INA226 Init Failed! Check wiring or address.\r\n");
        while(1);
    }
	MFRC522_Init(); // ��ʼ�� RFID
	
	// 【关键诊断】读取固件版本寄存器 (地址 0x37)
    // 正常的 MFRC522 应该返回 0x91 或 0x92
    uint8_t version = MFRC522_ReadReg(0x37); 
    printf("MFRC522 Version Register: 0x%02X\r\n", version);
    
    if (version == 0x00 || version == 0xFF) {
        
        
        // 进入死循环，方便你观察错误
        while(1) {
            HAL_Delay(1000);
            printf("Waiting for fix...\r\n");
        }
    } else {
        printf("[SUCCESS] RFID : 0x%02X\r\n", version);
    }
	
	
	
	
	
	
	
	
	
	
	
    uint8_t status;
    uint8_t tagType[2];
    uint8_t uid[5];


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

		// ================= RFID Logic Start =================
  
    uint8_t dataBlock[16];
    // Default Key A (Factory Default)
    uint8_t keyDefault[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 

    // 1. Request Card (Check for cards in the field)
    // PICC_REQIDL: Only check cards that are not in halt state
    status = MFRC522_Request(PICC_REQIDL, tagType);
    
    if (status == MI_OK) {
        printf("[RFID] Card Detected! Type: 0x%02X 0x%02X\r\n", tagType[0], tagType[1]);

        // 2. Anti-collision: Get Card UID
        status = MFRC522_Anticoll(uid);
        if (status == MI_OK) {
            // Print UID (First 4 bytes are the unique ID)
            printf("[RFID] UID: %02X %02X %02X %02X\r\n", uid[0], uid[1], uid[2], uid[3]);

            // 3. Select Card (Mandatory before Auth/Read/Write)
            status = MFRC522_SelectCard(uid);
            if (status == MI_OK) {
                printf("[RFID] Card Selected.\r\n");

                // 4. Authenticate Sector/Block
                // Trying to authenticate Block 1 with Key A (Default FF...)
                // Note: Block 0 is manufacturer block (read-only), Block 1 is safe for testing
                status = MFRC522_Auth(PICC_AUTHENT1A, 1, keyDefault, uid);
                
                if (status == MI_OK) {
                    printf("[RFID] Authentication Success (Key A).\r\n");

                    // 5. Read Block Data
                    status = MFRC522_ReadBlock(1, dataBlock);
                    if (status == MI_OK) {
                        printf("[RFID] Read Block 1 Success: ");
                        for(int i = 0; i < 16; i++) {
                            printf("%02X ", dataBlock[i]);
                        }
                        printf("\r\n");
                    } else {
                        printf("[RFID] Read Block Failed! Error: %d\r\n", status);
                    }

                } else {
                    printf("[RFID] Authentication Failed! Check Key or Card Type.\r\n");
                    // Some compatible chips or encrypted cards might need different keys
                }
                
                // 6. Halt Card (Put card to sleep to avoid repeated reads of same card)
                MFRC522_Halt();
                printf("[RFID] Card Halted. Waiting for next card...\r\n\r\n");
                
                // Delay to prevent UART flooding while card is present
                HAL_Delay(800); 
            } else {
                printf("[RFID] Select Card Failed!\r\n");
            }
        } else {
            printf("[RFID] Anti-collision Failed!\r\n");
        }
    }
    // ================= RFID Logic End =================


    // INA226 Logic (Existing)
    if (INA226_ReadData(&ina_data) == INA226_OK)
    {
        // Print INA226 data
//        printf("Vbus: %.2f V, Current: %.2f mA, Power: %.2f mW\r\n",
//               ina_data.voltage_bus,
//               ina_data.current_ma,
//               ina_data.power_mw);

        // Update OLED (Optional: You can add RFID info here too)
        OLED_Clear();
        OLED_ShowString(0, 8, (unsigned char *)"INA226", 16, 1);
        OLED_ShowFloat(0, 24, ina_data.voltage_bus, 2, 16);
        OLED_ShowString(60, 24, (unsigned char *)"V", 16, 1); 
        OLED_Refresh();
    }

    // Main loop delay
    HAL_Delay(100);
		 
  /* USER CODE END 3 */
}
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
