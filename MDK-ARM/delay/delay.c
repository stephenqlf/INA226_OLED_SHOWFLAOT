#include "delay.h"
#include "stm32f4xx.h"

void delay_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; 
    // DWT->LAR = 0xC5ACCE55; // Remove or comment this line if it errors
    DWT->CYCCNT = 0;       
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;           
}

void delay_us(uint32_t nus) {
    uint32_t startTick = DWT->CYCCNT;
    uint32_t delayTicks = nus * (SystemCoreClock / 1000000);
    while (DWT->CYCCNT - startTick < delayTicks);
}

// ºÁÃëÑÓÊ±
void delay_ms(uint16_t nms) {
    for(uint16_t i = 0; i < nms; i++) {
        delay_us(1000);
    }
}
