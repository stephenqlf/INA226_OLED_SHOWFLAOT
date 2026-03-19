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

// ========== 优化 RFID 初始化与诊断 ==========
MFRC522_Init(); // 初始化 RFID

// 读取固件版本寄存器 (地址 0x37)
uint8_t version = MFRC522_ReadReg(0x37); 
printf("\n=== MFRC522 Diagnostic ===\r\n");
printf("MFRC522 Version Register: 0x%02X\r\n", version);

// 版本号判断：只要不是 0x00/0xFF 就视为通信成功
if (version == 0x00 || version == 0xFF) {
    printf("[ERROR] MFRC522 Communication Failed! Check wiring.\r\n");
    while(1) {
        HAL_Delay(1000);
        printf("Waiting for fix...\r\n");
    }
} else {
    printf("[SUCCESS] MFRC522 Communication OK! Version: 0x%02X\r\n", version);
    printf("================================\r\n\n");
}

uint8_t status;
uint8_t tagType[2];
uint8_t uid[5];
uint8_t dataBlock[16];
uint8_t keyDefault[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // 默认密钥A
/* USER CODE END 2 */

/* Infinite loop */
while (1)
{
    uint8_t status;
    uint8_t tagType[2];
    uint8_t uid[5];

    // ========== RFID 核心读卡逻辑（最简、最高成功率） ==========
    // 1. 寻卡：PICC_REQALL 寻所有卡（包括休眠卡，成功率最高）
    status = MFRC522_Request(PICC_REQALL, tagType);
    
    if (status == MI_OK) {
        printf("\n================================\r\n");
        printf("[RFID] 检测到IC卡！卡片类型: 0x%02X 0x%02X\r\n", tagType[0], tagType[1]);
        
        // 2. 防冲突：获取卡片唯一UID
        status = MFRC522_Anticoll(uid);
        if (status == MI_OK) {
            printf("[RFID] 卡片UID：%02X %02X %02X %02X\r\n", 
                   uid[0], uid[1], uid[2], uid[3]);
            
            // 3. 休眠卡片（避免重复读取同一张卡）
            MFRC522_Halt();
            printf("[RFID] 卡片已休眠，等待下一张卡...\r\n");
            printf("================================\r\n\n");
            HAL_Delay(1500); // 延时1.5秒，方便查看日志
        }
    }

    // ========== INA226 电压读取（稳定显示） ==========
    if (INA226_ReadData(&ina_data) == INA226_OK)
    {
        printf("INA226: Voltage=%.2fV\r\n", ina_data.voltage_bus);
    }

    HAL_Delay(300); // 降低循环频率，避免串口刷屏
}
  /* USER CODE END 3 */
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
