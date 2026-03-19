#include "printf.h"
#include "string.h"
#include <stdio.h>
#include "usart.h"
//#include "usbd_cdc_if.h"  // 必须包含这个头文件


#ifdef __GNUC__
  /* With GCC, small printf (option LD Linker->Libraries->Small printf
     set to 'Yes') calls __io_putchar() */
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */



PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the EVAL_COM1 and Loop until the end of transmission */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}



//PUTCHAR_PROTOTYPE
//{
//  uint8_t ch_transmit = (uint8_t)ch;
//  
//  // 1. 处理换行符：Windows 串口助手需要 \r\n 才能换行并刷新缓冲区
//  if (ch == '\n') {
//    uint8_t cr = '\r';
//    // 先发送 \r
//    while (CDC_Transmit_FS(&cr, 1) != USBD_OK) {
//      // 简单等待，防止死锁可加计数超时
//      // 如果 USB 未连接，这里会一直阻塞，调试时请注意
//    }
//  }

//  // 2. 发送当前字符
//  // 增加一个简单的超时机制，防止 USB 未连接时程序死在这里
//  uint32_t timeout = 10000; 
//  while (CDC_Transmit_FS(&ch_transmit, 1) != USBD_OK) {
//    if (timeout-- == 0) return ch; // 超时退出，避免死机
//  }

//  return ch;
//}

