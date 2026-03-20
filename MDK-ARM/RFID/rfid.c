#include "rfid.h"

extern SPI_HandleTypeDef hspi1;

// SPI Low Level Logic
static uint8_t SPI_RW(uint8_t data) {
    uint8_t rxData;
    HAL_SPI_TransmitReceive(&hspi1, &data, &rxData, 1, 10);
    return rxData;
}

void RFID_WriteReg(uint8_t addr, uint8_t val) {
    HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_RESET);
    SPI_RW((addr << 1) & 0x7E);
    SPI_RW(val);
    HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_SET);
}

uint8_t RFID_ReadReg(uint8_t addr) {
    uint8_t val;
    HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_RESET);
    SPI_RW(((addr << 1) & 0x7E) | 0x80);
    val = SPI_RW(0x00);
    HAL_GPIO_WritePin(MFRC522_CS_PORT, MFRC522_CS_PIN, GPIO_PIN_SET);
    return val;
}

void RFID_SetBit(uint8_t reg, uint8_t mask) {
    RFID_WriteReg(reg, RFID_ReadReg(reg) | mask);
}

void RFID_ClearBit(uint8_t reg, uint8_t mask) {
    RFID_WriteReg(reg, RFID_ReadReg(reg) & (~mask));
}

void RFID_Init(void) {
    // 1. Hardware Reset (PB0)
    HAL_GPIO_WritePin(MFRC522_RST_PORT, MFRC522_RST_PIN, GPIO_PIN_RESET);
    delay_ms(50);
    HAL_GPIO_WritePin(MFRC522_RST_PORT, MFRC522_RST_PIN, GPIO_PIN_SET);
    delay_ms(50);

    // 2. Soft Reset
    RFID_WriteReg(CommandReg, PCD_RESETPHASE);

    // 3. Config
    RFID_WriteReg(TModeReg, 0x8D);
    RFID_WriteReg(TPrescalerReg, 0x3E);
    RFID_WriteReg(TReloadRegL, 30);
    RFID_WriteReg(TReloadRegH, 0);
    RFID_WriteReg(TxASKReg, 0x40);
    RFID_WriteReg(ModeReg, 0x3D);
    
    // Antenna ON
    RFID_SetBit(TxControlReg, 0x03);
}

// Internal Communication logic
static uint8_t RFID_ToCard(uint8_t cmd, uint8_t *pIn, uint8_t inLen, uint8_t *pOut, uint32_t *pOutLen) {
    uint8_t status = MI_ERR;
    uint8_t waitIRq = 0x30;
    
    RFID_WriteReg(CommandReg, PCD_IDLE);
    RFID_WriteReg(ComIrqReg, 0x7F);
    RFID_SetBit(FIFOLevelReg, 0x80); // Flush FIFO
    
    for (uint8_t i = 0; i < inLen; i++) RFID_WriteReg(FIFODataReg, pIn[i]);
    
    RFID_WriteReg(CommandReg, cmd);
    if (cmd == PCD_TRANSCEIVE) RFID_SetBit(BitFramingReg, 0x80);
    
    uint16_t timeout = 2000; 
    uint8_t n;
    do {
        n = RFID_ReadReg(ComIrqReg);
        timeout--;
    } while (timeout && !(n & 0x01) && !(n & waitIRq));

    RFID_ClearBit(BitFramingReg, 0x80);

    if (timeout != 0 && !(RFID_ReadReg(ErrorReg) & 0x1B)) {
        status = MI_OK;
        if (cmd == PCD_TRANSCEIVE) {
            n = RFID_ReadReg(FIFOLevelReg);
            uint8_t lastBits = RFID_ReadReg(ControlReg) & 0x07;
            *pOutLen = lastBits ? (n - 1) * 8 + lastBits : n * 8;
            if (n == 0) n = 1;
            for (uint8_t i = 0; i < n; i++) pOut[i] = RFID_ReadReg(FIFODataReg);
        }
    }
    return status;
}

uint8_t RFID_Request(uint8_t reqMode, uint8_t *TagType) {
    uint32_t len;
    RFID_WriteReg(BitFramingReg, 0x07);
    TagType[0] = reqMode;
    return RFID_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &len);
}

uint8_t RFID_Anticoll(uint8_t *Snr) {
    uint32_t len;
    RFID_WriteReg(BitFramingReg, 0x00);
    Snr[0] = PICC_ANTICOLL;
    Snr[1] = 0x20;
    return RFID_ToCard(PCD_TRANSCEIVE, Snr, 2, Snr, &len);
}
