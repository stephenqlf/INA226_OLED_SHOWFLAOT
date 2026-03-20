#ifndef __RFID_H
#define __RFID_H

#include "stm32f4xx_hal.h"
#include "delay.h"

// --- Hardware Pins ---
#define MFRC522_CS_PORT     GPIOA
#define MFRC522_CS_PIN      GPIO_PIN_4
#define MFRC522_RST_PORT    GPIOB
#define MFRC522_RST_PIN     GPIO_PIN_0

// --- Status Codes ---
#define MI_OK               0
#define MI_NOTAGERR         1
#define MI_ERR              2

// --- MFRC522 Commands ---
#define PCD_IDLE            0x00
#define PCD_AUTHENT         0x0E
#define PCD_RECEIVE         0x08
#define PCD_TRANSMIT        0x04
#define PCD_TRANSCEIVE      0x0C
#define PCD_RESETPHASE      0x0F
#define PCD_CALCCRC         0x03

// --- Mifare_One Card Commands ---
#define PICC_REQIDL         0x26
#define PICC_REQALL         0x52
#define PICC_ANTICOLL       0x93
#define PICC_SELECTTAG      0x93
#define PICC_AUTHENT1A      0x60
#define PICC_AUTHENT1B      0x61
#define PICC_READ           0x30
#define PICC_WRITE          0xA0
#define PICC_HALT           0x50

// --- MFRC522 Registers ---
#define CommandReg          0x01
#define ComIEnReg           0x02
#define DivIEnReg           0x03
#define ComIrqReg           0x04
#define DivIrqReg           0x05
#define ErrorReg            0x06
#define Status1Reg          0x07
#define Status2Reg          0x08
#define FIFODataReg         0x09
#define FIFOLevelReg        0x0A
#define WaterLevelReg       0x0B
#define ControlReg          0x0C
#define BitFramingReg       0x0D
#define CollReg             0x0E
#define ModeReg             0x11
#define TxModeReg           0x12
#define RxModeReg           0x13
#define TxControlReg        0x14
#define TxASKReg            0x15
#define TModeReg            0x2A
#define TPrescalerReg       0x2B
#define TReloadRegH         0x2C
#define TReloadRegL         0x2D

// --- Function Prototypes ---
void RFID_Init(void);
void RFID_WriteReg(uint8_t addr, uint8_t val);
uint8_t RFID_ReadReg(uint8_t addr);
uint8_t RFID_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t RFID_Anticoll(uint8_t *Snr);

#endif
