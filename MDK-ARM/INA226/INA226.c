#include "INA226.h"
#include "delay.h" // 确保 delay_us(3) 有效
#include "printf.h"
/* 内部校准值缓存 */
static uint16_t cal_value = 0;
static float current_lsb_ma = 0;

/* ================= 底层 GPIO 控制 (开漏模式) ================= */
// 使用开漏模式后，只需 SDA_HIGH() 即可释放总线供从机拉低，无需切换 MODE
#define SCL_HIGH()  HAL_GPIO_WritePin(INA226_SCL_PORT, INA226_SCL_PIN, GPIO_PIN_SET)
#define SCL_LOW()   HAL_GPIO_WritePin(INA226_SCL_PORT, INA226_SCL_PIN, GPIO_PIN_RESET)
#define SDA_HIGH()  HAL_GPIO_WritePin(INA226_SDA_PORT, INA226_SDA_PIN, GPIO_PIN_SET)
#define SDA_LOW()   HAL_GPIO_WritePin(INA226_SDA_PORT, INA226_SDA_PIN, GPIO_PIN_RESET)
#define READ_SDA()  HAL_GPIO_ReadPin(INA226_SDA_PORT, INA226_SDA_PIN)

static void I2C_Delay(void) {
    delay_us(6); // F4主频下建议 3-5us
}

/* ================= 软件 I2C 协议层 ================= */

void INA226_IO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能 GPIO 时钟 (根据实际端口修改)
    if(INA226_SCL_PORT == GPIOA || INA226_SDA_PORT == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    
    // 配置为开漏输出 (关键：Open-Drain 允许双向通信而无需频繁切换 Mode)
    GPIO_InitStruct.Pin = INA226_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD; 
    GPIO_InitStruct.Pull = GPIO_PULLUP; // 依靠内部或外部上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(INA226_SCL_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = INA226_SDA_PIN;
    HAL_GPIO_Init(INA226_SDA_PORT, &GPIO_InitStruct);
    
    SDA_HIGH();
    SCL_HIGH();
}

static void I2C_Start(void) {
    SDA_HIGH(); SCL_HIGH(); I2C_Delay();
    SDA_LOW();  I2C_Delay();
    SCL_LOW();  I2C_Delay();
}

static void I2C_Stop(void) {
    SDA_LOW();  SCL_LOW();  I2C_Delay();
    SCL_HIGH(); I2C_Delay();
    SDA_HIGH(); I2C_Delay();
}

static uint8_t I2C_WaitAck(void) {
    uint8_t ack;
    SDA_HIGH(); I2C_Delay(); // 释放 SDA 供从机控制
    SCL_HIGH(); I2C_Delay();
    ack = READ_SDA();
    SCL_LOW();  I2C_Delay();
    return ack;
}

static void I2C_SendAck(uint8_t ack) {
    if(ack) SDA_HIGH(); else SDA_LOW();
    I2C_Delay();
    SCL_HIGH(); I2C_Delay();
    SCL_LOW();  I2C_Delay();
    SDA_HIGH();
}

static uint8_t I2C_WriteByte(uint8_t byte) {
    for(int i = 0; i < 8; i++) {
        if(byte & 0x80) SDA_HIGH(); else SDA_LOW();
        byte <<= 1;
        I2C_Delay();
        SCL_HIGH(); I2C_Delay();
        SCL_LOW();  I2C_Delay();
    }
    return I2C_WaitAck();
}

static uint8_t I2C_ReadByte(uint8_t ack) {
    uint8_t byte = 0;
    SDA_HIGH(); // Release SDA
    I2C_Delay(); 
    for(int i = 0; i < 8; i++) {
        byte <<= 1;
        SCL_HIGH();
        I2C_Delay(); // Give the chip time to put data on the line
        if(READ_SDA()) byte |= 0x01;
        SCL_LOW();
        I2C_Delay();
    }
    I2C_SendAck(ack);
    return byte;
}


/* ================= INA226 寄存器操作 ================= */

static uint8_t INA226_WriteReg(uint8_t reg, uint16_t data) {
    I2C_Start();
    if(I2C_WriteByte(INA226_ADDR << 1)) { I2C_Stop(); return 1; }
    I2C_WriteByte(reg);
    I2C_WriteByte((data >> 8) & 0xFF);
    I2C_WriteByte(data & 0xFF);
    I2C_Stop();
    return 0;
}

static uint8_t INA226_ReadReg(uint8_t reg, uint16_t *data) {
    I2C_Start();
    if(I2C_WriteByte(INA226_ADDR << 1)) { I2C_Stop(); return 1; }
    I2C_WriteByte(reg);
    
    I2C_Start(); // 重复启动
    I2C_WriteByte((INA226_ADDR << 1) | 0x01);
    uint8_t msb = I2C_ReadByte(0); // ACK
    uint8_t lsb = I2C_ReadByte(1); // NACK
    I2C_Stop();
    
    *data = (uint16_t)(msb << 8) | lsb;
    return 0;
}

/* ================= 业务逻辑 ================= */

INA226_Status_t INA226_Init(void) {
    uint16_t id = 0;
    INA226_IO_Init();
    delay_ms(10); // Wait for chip to stabilize

    // 1. Software Reset (Optional but recommended)
    INA226_WriteReg(INA226_REG_CONFIG, 0x8000); 
    delay_ms(10);

    // 2. Read ID with a retry loop
    for(int i = 0; i < 3; i++) {
        if(INA226_ReadReg(INA226_REG_ID, &id) == 0) {
            if(id == 0x2260) break;
        }
        delay_ms(5);
    }

//    if(id != 0x2260) {
//        printf("ID Error! Read: 0x%04X, Expected: 0x2260\r\n", id); // Debug output
//        return INA226_ERROR_ID;
//    }

    // 3. Calibration
    float current_lsb = INA226_MAX_CURRENT / 32768.0f;
    cal_value = (uint16_t)(0.00512f / (current_lsb * INA226_SHUNT_RES));
    current_lsb_ma = current_lsb * 1000.0f;

    INA226_WriteReg(INA226_REG_CALIBRATION, cal_value);
    INA226_WriteReg(INA226_REG_CONFIG, INA226_CONFIG_SET);

    return INA226_OK;
}

INA226_Status_t INA226_ReadData(INA226_Data_t *data) {
    uint16_t raw;
    if(!data) return INA226_ERROR_PARAM;

    // 总线电压 LSB = 1.25mV
    if(INA226_ReadReg(INA226_REG_BUS_VOLT, &raw) == 0)
        data->voltage_bus = raw * 0.00125f;

    // 分流电压 LSB = 2.5uV
    if(INA226_ReadReg(INA226_REG_SHUNT_VOLT, &raw) == 0)
        data->voltage_shunt = (int16_t)raw * 0.0025f;

    // 电流 (需配合校准值)
    if(INA226_ReadReg(INA226_REG_CURRENT, &raw) == 0)
        data->current_ma = (int16_t)raw * current_lsb_ma;

    // 功率 LSB = 25 * Current_LSB
    if(INA226_ReadReg(INA226_REG_POWER, &raw) == 0)
        data->power_mw = raw * (current_lsb_ma * 25.0f);

    return INA226_OK;
}
void INA226_Searching(void){


printf("Scanning I2C bus...\r\n");
for (uint8_t i = 0; i < 128; i++) {
    I2C_Start(); 
    if (I2C_WriteByte(i << 1) == 0) { 
        printf("Found I2C device at: 0x%02X\r\n", i);
    }
    I2C_Stop();
    delay_ms(5);
}








}
	
