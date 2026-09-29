#ifndef SYTICK_H
#define SYTICK_H
#include <stdint.h>

extern volatile uint32_t millis;
void timeInit(void);
void SysTick_Handler(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

#endif // SYTICK_H
