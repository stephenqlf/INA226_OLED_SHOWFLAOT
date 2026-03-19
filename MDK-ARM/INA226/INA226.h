#ifndef __INA226_H
#define __INA226_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ================= 用户配置区域 ================= */

// 引脚定义 (可根据实际情况修改)
#define INA226_SCL_PORT     GPIOB
#define INA226_SCL_PIN      GPIO_PIN_1

#define INA226_SDA_PORT     GPIOB
#define INA226_SDA_PIN      GPIO_PIN_2

// INA226 I2C 7位地址 (A1=GND, A0=GND 时为 0x40)
#define INA226_ADDR         0x40 

// 硬件参数
#define INA226_SHUNT_RES    0.01f   // 分流电阻值 (Ω)
#define INA226_MAX_CURRENT  10.0f   // 最大预期电流 (A)

/* ================= 寄存器定义 ================= */
#define INA226_REG_CONFIG       0x00
#define INA226_REG_SHUNT_VOLT   0x01
#define INA226_REG_BUS_VOLT     0x02
#define INA226_REG_POWER        0x03
#define INA226_REG_CURRENT      0x04
#define INA226_REG_CALIBRATION  0x05
#define INA226_REG_MASK_ENABLE  0x06
#define INA226_REG_ALERT_LIMIT  0x07
#define INA226_REG_ID           0xFE

/* 配置位定义 (AVG:16次, Vbus:1.1ms, Vsh:1.1ms, 连续采样模式) */
#define INA226_CONFIG_SET       0x4527 

/* ================= 数据结构 ================= */
typedef enum {
    INA226_OK = 0,
    INA226_ERROR_IO,
    INA226_ERROR_ID,
    INA226_ERROR_PARAM
} INA226_Status_t;

typedef struct {
    float voltage_bus;    // 总线电压 (V)
    float voltage_shunt;  // 分流电压 (mV)
    float current_ma;     // 电流 (mA)
    float power_mw;       // 功率 (mW)
} INA226_Data_t;

/* ================= 函数声明 ================= */
 INA226_Status_t INA226_Init(void);
INA226_Status_t INA226_ReadData(INA226_Data_t *data);
void INA226_Searching(void);
void INA226_IO_Init(void);	
#endif

