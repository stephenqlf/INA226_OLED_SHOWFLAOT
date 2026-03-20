/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Optimized RFID with OLED Display & Debounce)
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
#include <stdio.h>
#include <string.h>
#include "oled.h"
#include "INA226.h"
#include "delay.h"
#include "rfid.h"

// 【修复点 1】兼容 Keil V5 (ARMCC) 的 bool 类型
// 如果编译器不支持 stdbool.h，则手动定义
#if !defined(__cplusplus) && !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 199901L)
  #ifndef bool
    #define bool uint8_t
    #define true 1
    #define false 0
  #endif
#else
  #include <stdbool.h>
#endif

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

// 用于去重逻辑的全局变量
uint8_t last_uid[5] = {0};      // 存储上一次的 UID
uint8_t current_uid[5] = {0};   // 存储当前的 UID
bool is_card_present = false;   // 标记当前是否有卡
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
// 辅助函数：比较两个 UID 是否相同
bool CompareUID(uint8_t *uid1, uint8_t *uid2) {
    for (int i = 0; i < 4; i++) {
        if (uid1[i] != uid2[i]) return false;
    }
    return true;
}

// 【修复点 2】修复 OLED_ShowString 参数类型警告
// 将 const char* 强制转换为 u8* 以匹配你的 OLED 库定义
void UpdateOLED_Status(const char* line1, const char* line2, const char* line3) {
    OLED_Clear();
    if(line1) OLED_ShowString(0, 0, (u8*)line1, 16, 1);
    if(line2) OLED_ShowString(0, 16, (u8*)line2, 16, 1);
    if(line3) OLED_ShowString(0, 32, (u8*)line3, 16, 1);
}
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
  
  // --- 1. OLED 初始化 ---
  OLED_Init();
  OLED_Clear();
  OLED_ShowString(0, 0, (u8*)"System Starting...", 16, 1);
  
  // --- 2. INA226 初始化 ---
  INA226_IO_Init();
  INA226_Searching();
  
  char ina_status_str[20];
  if (INA226_Init() == INA226_OK) {
      printf("INA226 Init Success!\r\n");
      sprintf(ina_status_str, "INA226: OK");
  } else {
      printf("INA226 Init Failed!\r\n");
      sprintf(ina_status_str, "INA226: FAIL");
  }
  
  // --- 3. RFID 初始化与诊断 ---
  RFID_Init(); 
  uint8_t version = RFID_ReadReg(0x37); // VersionReg address
  printf("RC522 Version Register: 0x%02X\r\n", version);

  char rfid_status_str[20];
  // 支持 0x91, 0x92 (NXP) 和 0x82 (Clone)
  if (version == 0x91 || version == 0x92 || version == 0x82) {
      printf("[SUCCESS] RFID Comm OK! Ver: 0x%02X\r\n", version);
      sprintf(rfid_status_str, "RFID: OK (0x%02X)", version);
  } else {
      printf("[ERROR] RFID Comm Failed! Got: 0x%02X\r\n", version);
      sprintf(rfid_status_str, "RFID: FAIL (0x%02X)", version);
      // 如果 RFID 失败，显示错误并卡死，方便调试
      UpdateOLED_Status(rfid_status_str, "Check Wiring!", NULL);
      while(1); 
  }

  // 显示初始状态页
  UpdateOLED_Status(rfid_status_str, ina_status_str, "Waiting for Card...");
  HAL_Delay(1000); // 让用户看清初始化结果
  
  // 清空缓冲区，准备进入主循环
  memset(last_uid, 0, 5);
  
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint8_t status;
    uint8_t tagType[2];
    
    // 1. 尝试寻卡
    status = RFID_Request(PICC_REQIDL, tagType);
    
    if (status == MI_OK) {
        // 2. 防冲突获取 UID
        status = RFID_Anticoll(current_uid);
        
        if (status == MI_OK) {
            // 成功读取到 UID
            
            // 检查是否是同一张卡 (去重)
            if (!is_card_present || !CompareUID(last_uid, current_uid)) {
                // 情况 A: 新卡片插入 或 卡片更换
                
                is_card_present = true;
                
                // 复制当前 UID 到 last_uid
                memcpy(last_uid, current_uid, 4);
                
                // --- 硬件反馈 ---
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5); // 翻转 LED
                
                // --- 串口打印 ---
                printf("\n[NEW CARD] Detected!\r\n");
                printf("UID: %02X %02X %02X %02X\r\n", current_uid[0], current_uid[1], current_uid[2], current_uid[3]);
                
                // --- OLED 显示更新 ---
                char uid_buf[20];
                sprintf(uid_buf, "%02X %02X %02X %02X", current_uid[0], current_uid[1], current_uid[2], current_uid[3]);
                
                OLED_Clear(); // 清屏以防残留
                OLED_ShowString(0, 0, (u8*)"Card Detected!", 16, 1);
                OLED_ShowString(0, 16, (u8*)"UID:", 16, 1);
                // 【修复点 3】强制转换 uid_buf 为 u8*
                OLED_ShowString(48, 16, (u8*)uid_buf, 16, 1); 
            } else {
                // 情况 B: 同一张卡持续存在，不做任何操作 (防止刷屏)
            }
        } else {
            // 防冲突失败，可能是干扰或卡片移开过快
            is_card_present = false;
        }
    } else {
        // 未检测到卡片
        if (is_card_present) {
            // 情况 C: 卡片被移除
            is_card_present = false;
            printf("[INFO] Card Removed.\r\n");
            
            // 恢复 OLED 到等待界面
            OLED_Clear();
            // 【修复点 4】强制转换状态字符串
            OLED_ShowString(0, 0, (u8*)rfid_status_str, 12, 1); 
            OLED_ShowString(0, 16, (u8*)ina_status_str, 12, 1);
            OLED_ShowString(0, 32, (u8*)"Waiting...", 16, 1);
        }
    }
    
    // 适当延时，降低轮询频率
    HAL_Delay(100); 
    /* USER CODE END WHILE */
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