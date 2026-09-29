#include "adc.h"
#include "stm32f401xc.h"
#include <stdint.h>

void ADCInit(void) {
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  GPIOA->MODER &= ~0x3FU; // CLEAR
  GPIOA->MODER |= 0x3FU;  // Set as Analog
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
  ADC1->CR1 &= ~(0x3U << 24);  // resolution 12bit
  ADC1->CR1 &= ~(0x1U << 8);   // SCAN Mode (disable)
  ADC1->SQR1 &= ~(0xFU << 20); // on conversion
  ADC1->CR2 |= 0x1U;           // on ADC
}

void adcCH0Raw(uint16_t *adcnum) {
  ADC1->SQR3 &= ~0x1FU;             // select chanel 0
  ADC1->CR2 |= ADC_CR2_SWSTART;     // START conversion
  while (!(ADC1->SR & (0x1U << 1))) // wait for da conversion completed
    ;
  *adcnum = (uint16_t)ADC1->DR;
}

void adcCH1Raw(uint16_t *adcnum) {
  ADC1->SQR3 &= ~0x1FU;             // clear
  ADC1->SQR3 |= 0x01;               // set chanel 1
  ADC1->CR2 |= ADC_CR2_SWSTART;     // START conversion
  while (!(ADC1->SR & (0x1U << 1))) // wait for da conversion completed
    ;
  *adcnum = (uint16_t)ADC1->DR;
}

void adcCH2Raw(uint16_t *adcnum) {
  ADC1->SQR3 &= ~0x1FU;             // clear
  ADC1->SQR3 |= 0x02;               // set chanel 2
  ADC1->CR2 |= ADC_CR2_SWSTART;     // START conversion
  while (!(ADC1->SR & (0x1U << 1))) // wait for da conversion completed
    ;
  *adcnum = (uint16_t)ADC1->DR;
}
