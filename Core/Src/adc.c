#include "adc.h"
#include "stm32f401xc.h"
#include "watchdog.h"
#include <stdint.h>

void ADCInit(void) {
  // 1. Enable GPIOA clock and configure PA0, PA1, PA2 as Analog mode
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  GPIOA->MODER &= ~0x3FU; // Clear mode bits for PA0..PA2
  GPIOA->MODER |= 0x3FU;  // Set PA0, PA1, PA2 to Analog mode (11b)

  // 2. Enable ADC1 clock
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

  // 3. Configure ADC1: 12-bit resolution, single-conversion mode
  ADC1->CR1 &= ~(0x3U << 24);  // 12-bit resolution
  ADC1->CR1 &= ~(0x1U << 8);   // Disable SCAN mode
  ADC1->SQR1 &= ~(0xFU << 20); // 1 conversion in sequence

  // 4. Turn on ADC
  ADC1->CR2 |= 0x1U;
}

static uint16_t adcReadChannel(uint8_t channel) {
  // Select regular sequence channel
  ADC1->SQR3 = channel & 0x1FU;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < 4; i++) {
    // Start conversion
    ADC1->CR2 |= ADC_CR2_SWSTART;

    // Wait for EOC (End of Conversion)
    while (!(ADC1->SR & (1U << 1))) {
      watchdogRefresh();
    }

    // Reading DR automatically clears EOC flag
    sum += (uint16_t)ADC1->DR;
  }

  // Rounded 4-sample average
  return (uint16_t)((sum + 2) / 4);
}

void adcCH0Raw(uint16_t *adcnum) {
  *adcnum = adcReadChannel(0);
}

void adcCH1Raw(uint16_t *adcnum) {
  *adcnum = adcReadChannel(1);
}

void adcCH2Raw(uint16_t *adcnum) {
  *adcnum = adcReadChannel(2);
}
