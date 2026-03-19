/* 
 * File: rfid.h
 * Description: MFRC522 RFID Driver Header for STM32F4
 * 注意：确保 main.h 中已定义 SystemCoreClock 和 SPI 句柄 hspi1
 */

#ifndef __RFID_H
#define __RFID_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

// ================= 硬件引脚配置 =================
// 请根据 CubeMX 生成的 gpio.h 或实际接线确认这些宏是否正确
#define MFRC522_CS_PORT    GPIOA
#define MFRC522_CS_PIN     GPIO_PIN_4
#define MFRC522_RST_PORT   GPIOB
#define MFRC522_RST_PIN    GPIO_PIN_0

// ================= 状态码定义 =================
#define MI_OK             0
#define MI_ERR            1
#define MI_NOTAGERR       2

// ================= 卡片命令常量 =================
#define PICC_REQIDL       0x26  // 寻天线区内未进入休眠状态的卡
#define PICC_REQALL       0x52  // 寻天线区内所有卡
#define PICC_ANTICOLL     0x93  // 防冲突命令
#define PICC_SElECTTAG    0x93  // 选卡命令
#define PICC_AUTHENT1A    0x60  // 验证A密钥
#define PICC_AUTHENT1B    0x61  // 验证B密钥
#define PICC_READ         0x30  // 读块
#define PICC_WRITE        0xA0  // 写块
#define PICC_DECREMENT    0xC0  // 扣款
#define PICC_INCREMENT    0xC1  // 充值
#define PICC_RESTORE      0xC2  // 将缓冲区数据写入块
#define PICC_TRANSFER     0xB0  // 将块数据读到缓冲区
#define PICC_HALT         0x50  // 休眠命令

// ================= 数据结构定义 =================
/**
 * @brief 卡片信息结构体
 * 用于在 main.c 中存储读取到的 UID 等信息
 */
typedef struct {
    uint8_t uid[5];   // 卡片 UID (通常 4 字节，第 5 字节为校验)
    uint8_t uidLen;   // UID 有效长度
} Mfrc522_CardInfo;

// ================= 函数声明 =================
// 注意：返回值类型必须与 rfid.c 中的定义严格一致

/**
 * @brief 初始化 MFRC522
 */
void MFRC522_Init(void);

/**
 * @brief 复位 MFRC522
 * @return MI_OK
 */
uint8_t MFRC522_Reset(void);

/**
 * @brief 寻卡
 * @param reqMode: 模式 (PICC_REQIDL 或 PICC_REQALL)
 * @param tagType: 返回卡片类型 (2 字节)
 * @return MI_OK 或 MI_ERR
 */
uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *tagType);

/**
 * @brief 防冲突，获取卡片 UID
 * @param uid: 返回 UID 缓冲区 (至少 5 字节)
 * @return MI_OK 或 MI_ERR
 */
uint8_t MFRC522_Anticoll(uint8_t *uid);

/**
 * @brief 选卡
 * @param uid: 卡片 UID (4 字节)
 * @return MI_OK 或 MI_ERR
 */
uint8_t MFRC522_SelectCard(uint8_t *uid);

/**
 * @brief 验证密码
 * @param authMode: PICC_AUTHENT1A 或 PICC_AUTHENT1B
 * @param blockAddr: 块地址
 * @param key: 密钥 (6 字节)
 * @param uid: 卡片 UID (4 字节)
 * @return MI_OK 或 MI_ERR
 */
uint8_t MFRC522_Auth(uint8_t authMode, uint8_t blockAddr, uint8_t *key, uint8_t *uid);

/**
 * @brief 读块
 * @param blockAddr: 块地址
 * @param data: 数据缓冲区 (16 字节)
 * @return MI_OK 或 MI_ERR
 */
uint8_t MFRC522_ReadBlock(uint8_t blockAddr, uint8_t *data);

/**
 * @brief 写块
 * @param blockAddr: 块地址
 * @param data: 数据缓冲区 (16 字节)
 * @return MI_OK 或 MI_ERR
 */
uint8_t MFRC522_WriteBlock(uint8_t blockAddr, uint8_t *data);

/**
 * @brief 休眠卡片
 * @return MI_OK
 */
uint8_t MFRC522_Halt(void);

#endif /* __RFID_H */
