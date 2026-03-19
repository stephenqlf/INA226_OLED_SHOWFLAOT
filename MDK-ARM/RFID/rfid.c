/* 
 * File: rfid.c
 * Description: MFRC522 RFID Driver for STM32F4 (HAL Library)
 * 修复说明：
 * 1. 修正 HAL 库头文件包含顺序
 * 2. 统一函数声明/定义的返回值类型 (uint8_t)
 * 3. 移除未使用的静态函数以消除警告
 * 4. 规范文件格式 (末尾换行)
 */

// ================= 头文件包含 (严格按依赖顺序) =================
#include "stm32f4xx_hal.h"      // 必须最先包含，获取 SPI_HandleTypeDef 定义
#include "stm32f4xx_hal_spi.h"  // 显式包含 SPI HAL 驱动
#include "main.h"               // 系统核心定义 (包含 SystemCoreClock)
#include "rfid.h"               // 自定义头文件
#include <string.h>             // 字符串处理

// ================= 关键变量声明 =================
// 声明外部 SPI 句柄（需在 main.c 或 spi.c 中定义 hspi1）
extern SPI_HandleTypeDef hspi1;

// SPI 句柄宏定义
#ifndef MFRC522_SPI_HANDLE
#define MFRC522_SPI_HANDLE hspi1
#endif

// 状态码定义 (如果 rfid.h 中已定义，这里不会冲突)
#ifndef MI_OK
#define MI_OK       0
#define MI_ERR      1
#define MI_NOTAGERR 2
#endif

// ================= 本地辅助函数 =================

/**
 * @brief 微秒级延时
 * @note 依赖 SystemCoreClock，确保在 SystemClock_Config() 之后调用
 */
static void Delay_us(uint32_t us) {
    uint32_t delay = (SystemCoreClock / 8000000 * us); 
    while(delay--);
}

/**
 * @brief 毫秒级延时
 */
static void Delay_ms(uint32_t ms) {
    for(uint32_t i = 0; i < ms; i++) {
        Delay_us(1000);
    }
}

// ================= 底层硬件操作 =================

/**
 * @brief 拉低片选信号
 */
static void MFRC522_CS_LOW(void) {
    HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_RESET);
}

/**
 * @brief 拉高片选信号
 */
static void MFRC522_CS_HIGH(void) {
    HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_SET);
}

/**
 * @brief 写寄存器
 * @param addr: 寄存器地址
 * @param val: 写入的值
 */
void MFRC522_WriteReg(uint8_t addr, uint8_t val) {
    // 地址位：最高位为 0，第 6 位为 0 (0x7E 掩码)
    uint8_t txData[2] = {(addr << 1) & 0x7E, val};
    
    MFRC522_CS_LOW();
    HAL_SPI_Transmit(&MFRC522_SPI_HANDLE, txData, 2, 100);
    MFRC522_CS_HIGH();
}

/**
 * @brief 读寄存器
 * @param addr: 寄存器地址
 * @return: 读取的值
 */
uint8_t MFRC522_ReadReg(uint8_t addr) {
    // 地址位：最高位为 1 (0x80)，表示读操作
    uint8_t txData = ((addr << 1) & 0x7E) | 0x80;
    uint8_t rxData = 0;
    
    MFRC522_CS_LOW();
    HAL_SPI_Transmit(&MFRC522_SPI_HANDLE, &txData, 1, 100);
    HAL_SPI_Receive(&MFRC522_SPI_HANDLE, &rxData, 1, 100);
    MFRC522_CS_HIGH();
    
    return rxData;
}

// ================= 核心逻辑函数 =================

/**
 * @brief 设置寄存器位掩码
 */
void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = MFRC522_ReadReg(reg);
    MFRC522_WriteReg(reg, tmp | mask);
}

/**
 * @brief 清除寄存器位掩码
 */
void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    uint8_t tmp = MFRC522_ReadReg(reg);
    MFRC522_WriteReg(reg, tmp & (~mask));
}

/**
 * @brief 计算 CRC16 (使用 MFRC522 硬件)
 */
void MFRC522_CalculateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData) {
    uint8_t i, n;
    
    MFRC522_ClearBitMask(0x05, 0x04); // IRQEn: 清除 CRCIrq
    MFRC522_SetBitMask(0x0A, 0x80);   // CommandReg: 启动 CRC 协处理器
    MFRC522_WriteReg(0x01, 0x00);     // DivIrqReg: 清零
    
    // 写入数据到 FIFO
    for (i = 0; i < len; i++) {
        MFRC522_WriteReg(0x09, pIndata[i]); // FIFO Data Reg
    }
    
    MFRC522_WriteReg(0x01, 0x03); // CommandReg = CalcCRC
    MFRC522_SetBitMask(0x05, 0x04); // 允许 CRC 中断
    
    i = 0xFF;
    do {
        n = MFRC522_ReadReg(0x05); // IrqReg
        i--;
    } while ((i != 0) && !(n & 0x04)); // 等待 CRCIrq 标志
    
    // 读取结果
    pOutData[0] = MFRC522_ReadReg(0x22); // CRCResultRegL
    pOutData[1] = MFRC522_ReadReg(0x23); // CRCResultRegH
}

/**
 * @brief MFRC522 与卡片通信的核心函数
 */
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint8_t *backLen) {
    uint8_t status = MI_ERR;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits;
    uint8_t n;
    uint32_t i;

    // 根据命令设置中断使能和等待标志
    switch (command) {
        case 0x0E: // MFCmdReq (Auth)
            irqEn = 0x12; 
            waitIRq = 0x10; 
            break; 
        case 0x04: // MFAnticoll (实际很少直接用 0x04 作为 ToCard 命令，通常用 0x0C Transceive)
        case 0x0C: // Transceive (通用收发)
            irqEn = 0x77; 
            waitIRq = 0x30; 
            break;
        default:
             irqEn = 0x77; 
             waitIRq = 0x30; 
             break;
    }

    // 配置 FIFO 和命令
    MFRC522_WriteReg(0x02, irqEn | 0x80); // CommIEnReg
    MFRC522_ClearBitMask(0x04, 0x80);     // CommIrqReg 清除所有中断
    MFRC522_SetBitMask(0x0A, 0x80);       // CommandReg 清空 FIFO
    MFRC522_WriteReg(0x01, 0x00);         // DivIrqReg 清零
    
    // 写入发送数据
    for (i = 0; i < sendLen; i++) {
        MFRC522_WriteReg(0x09, sendData[i]);
    }

    // 执行命令
    MFRC522_WriteReg(0x0A, command); // CommandReg (0x0A)
    if (command == 0x0C) { // Transceive
        MFRC522_SetBitMask(0x0D, 0x80); // TxControlReg 启动发射
    }

    // 等待命令完成
    i = 2000; // 超时计数
    do {
        n = MFRC522_ReadReg(0x04); // CommIrqReg
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq)); // 等待 CmdIrq 或 RxIrq

    MFRC522_ClearBitMask(0x0D, 0x80); // 关闭发射

    if (i != 0) {
        // 检查错误标志 (ErrorReg 0x08)
        if ((MFRC522_ReadReg(0x08) & 0x1B) == 0x00) { 
            status = MI_OK;
            if (n & irqEn & 0x01) { 
                status = MI_NOTAGERR; // No card error
            }
            
            if (command == 0x0C || command == 0x04) {
                n = MFRC522_ReadReg(0x09); // FIFOLevelReg
                lastBits = MFRC522_ReadReg(0x0C) & 0x07; // ControlReg - RxLastBits
                
                if (lastBits) {
                    *backLen = (n - 1) * 8 + lastBits;
                } else {
                    *backLen = n * 8;
                }

                if (n == 0) n = 1;
                if (n > 16) n = 16;
                
                // 读取接收数据
                for (i = 0; i < n; i++) {
                    backData[i] = MFRC522_ReadReg(0x09);
                }
            }
        } else {
            status = MI_ERR;
        }
    } else {
        status = MI_ERR; // Timeout
    }

    return status;
}

// ================= 业务功能函数 =================

/**
 * @brief 复位 MFRC522
 */
uint8_t MFRC522_Reset(void) {
    MFRC522_WriteReg(0x0A, 0x0F); // CommandReg: SoftReset
    return MI_OK;
}

/**
 * @brief 初始化 MFRC522
 */
void MFRC522_Init(void) {
    // 硬件复位
    HAL_GPIO_WritePin(MFRC522_RST_PORT, MFRC522_RST_PIN, GPIO_PIN_RESET);
    Delay_ms(10);
    HAL_GPIO_WritePin(MFRC522_RST_PORT, MFRC522_RST_PIN, GPIO_PIN_SET);
    Delay_ms(10);

    // 软件复位
    MFRC522_Reset();
    Delay_ms(10);

    // 定时器设置 (TimerPrescaler, TimerReload)
    MFRC522_WriteReg(0x2A, 0x8D); // TModeReg
    MFRC522_WriteReg(0x2B, 0x01); // TPrescalerReg
    MFRC522_WriteReg(0x2C, 0x3E); // TReloadRegH
    MFRC522_WriteReg(0x2D, 0x10); // TReloadRegL

    // TX 设置
    MFRC522_WriteReg(0x15, 0x40); // TxASKReg
    MFRC522_WriteReg(0x11, 0x3D); // ModeReg
    
    // 天线开启
    MFRC522_ClearBitMask(0x0D, 0x03); // TxControlReg
    MFRC522_SetBitMask(0x0D, 0x03);   // 开启天线
    
    // 设置 CRC 初始值
    MFRC522_WriteReg(0x26, 0x70); // RFCfgReg
}

/**
 * @brief 寻卡 (Request)
 */
uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *tagType) {
    uint8_t status;
    uint8_t backBits = 0;

    MFRC522_WriteReg(0x0D, 0x07); // TxControlReg
    
    // 发送命令：Transceive (0x0C)
    status = MFRC522_ToCard(0x0C, &reqMode, 1, tagType, &backBits);

    if (status != MI_OK || backBits != 0x10) {
        return MI_ERR;
    }
    
    return MI_OK;
}

/**
 * @brief 防冲突 (Anticoll)，获取卡片 UID
 */
uint8_t MFRC522_Anticoll(uint8_t *uid) {
    uint8_t status;
    uint8_t i;
    uint8_t serNumCheck = 0;
    uint8_t unLen = 0;

    MFRC522_ClearBitMask(0x0D, 0x80); // TxControlReg
    
    // 发送防冲突命令：0x93, 0x20
    uint8_t cmd[2] = {0x93, 0x20};
    status = MFRC522_ToCard(0x0C, cmd, 2, uid, &unLen);

    if (status == MI_OK && unLen == 0x35) { // 正常返回 5 字节
        // 计算校验和
        for (i = 0; i < 4; i++) {
            serNumCheck ^= uid[i];
        }
        if (serNumCheck != uid[4]) {
            status = MI_ERR;
        }
    } else {
        status = MI_ERR;
    }

    return status;
}

/**
 * @brief 选卡 (Select Card)
 * @note 内部直接实现了 BCC 计算，未调用外部静态函数以避免警告
 */
uint8_t MFRC522_SelectCard(uint8_t *uid) {
    uint8_t status;
    uint8_t i;
    uint8_t buf[9];
    uint8_t unLen = 0;
    uint8_t bcc = 0;

    buf[0] = 0x93; // Select Command
    buf[1] = 0x70; // Select Acknowledge
    
    // 填充 UID 并计算 BCC
    for (i = 0; i < 4; i++) {
        buf[i+2] = uid[i];
        bcc ^= uid[i];
    }
    buf[6] = bcc; // BCC

    status = MFRC522_ToCard(0x0C, buf, 7, buf, &unLen);

    if (status == MI_OK && unLen == 0x18) {
        return MI_OK;
    } else {
        return MI_ERR;
    }
}

/**
 * @brief 验证扇区密码 (Auth)
 */
uint8_t MFRC522_Auth(uint8_t authMode, uint8_t blockAddr, uint8_t *key, uint8_t *uid) {
    uint8_t status;
    uint8_t i;
    uint8_t buf[12];
    uint8_t unLen = 0;

    // 构建认证命令帧
    buf[0] = authMode;
    buf[1] = blockAddr;
    for (i = 0; i < 6; i++) {
        buf[i+2] = key[i];
    }
    for (i = 0; i < 4; i++) {
        buf[i+8] = uid[i];
    }

    status = MFRC522_ToCard(0x0E, buf, 12, buf, &unLen); 

    // 检查 Status2Reg (0x08) 的 AuthErr 位 (bit 3)
    if ((status == MI_OK) && (!(MFRC522_ReadReg(0x08) & 0x08))) { 
        return MI_OK;
    } else {
        return MI_ERR;
    }
}

/**
 * @brief 读块数据 (Read Block)
 */
uint8_t MFRC522_ReadBlock(uint8_t blockAddr, uint8_t *data) {
    uint8_t status;
    uint8_t unLen = 0;
    
    uint8_t cmd[2] = {0x30, blockAddr};
    
    status = MFRC522_ToCard(0x0C, cmd, 2, data, &unLen);

    if (status == MI_OK && unLen == 16) {
        return MI_OK;
    } else {
        return MI_ERR;
    }
}

/**
 * @brief 写块数据 (Write Block)
 */
uint8_t MFRC522_WriteBlock(uint8_t blockAddr, uint8_t *data) {
    uint8_t status;
    uint8_t i;
    uint8_t buf[18];
    uint8_t unLen = 0;
    uint8_t crc[2];

    // 构建写命令帧 (Write Command + Block Addr + CRC)
    buf[0] = 0xA0; 
    buf[1] = blockAddr;
    
    MFRC522_CalculateCRC(buf, 2, crc);
    buf[2] = crc[0];
    buf[3] = crc[1];

    // 发送写命令
    status = MFRC522_ToCard(0x0C, buf, 4, buf, &unLen);

    if ((status != MI_OK) || (unLen != 4) || ((buf[0] & 0x0F) != 0x0A)) {
        return MI_ERR;
    }

    // 发送实际数据 (Data + CRC)
    for (i = 0; i < 16; i++) {
        buf[i] = data[i];
    }
    
    MFRC522_CalculateCRC(buf, 16, crc);
    buf[16] = crc[0];
    buf[17] = crc[1];

    status = MFRC522_ToCard(0x0C, buf, 18, buf, &unLen);

    if ((status == MI_OK) && (unLen == 4) && ((buf[0] & 0x0F) == 0x0A)) {
        return MI_OK;
    } else {
        return MI_ERR;
    }
}

/**
 * @brief 停止卡片通信 (Halt)
 */
uint8_t MFRC522_Halt(void) {
    uint8_t unLen = 0;
    uint8_t buf[4];
    uint8_t crc[2];

    buf[0] = 0x50; // Halt Command
    buf[1] = 0x00;
    
    MFRC522_CalculateCRC(buf, 2, crc);
    buf[2] = crc[0];
    buf[3] = crc[1];

    // 忽略返回值，避免未使用变量警告
    (void)MFRC522_ToCard(0x0C, buf, 4, buf, &unLen); 
    
    return MI_OK;
}
// 文件末尾已保留换行符
