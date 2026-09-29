#ifndef ADC_H
#define ADC_H
#include <stdint.h>

// Function to initialize the ADC
void ADCInit(void);

void adcCH0Raw(uint16_t *adcnum);
void adcCH1Raw(uint16_t *adcnum);
void adcCH2Raw(uint16_t *adcnum);
#endif // !ADC_H
