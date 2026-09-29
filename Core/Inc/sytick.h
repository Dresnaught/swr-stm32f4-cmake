#ifndef SYTICK_H
#define SYTICK_H
#include <stdint.h>

extern volatile uint32_t now;
// Function to initialize the SysTick timer
void timeInit(void);
void SysTick_Handler(void);
// Function to create a delay in milliseconds
void delay_ms(uint32_t ms);
// Function to create a delay in microseconds
void delay_us(uint32_t us);

#endif // SYTICK_H
