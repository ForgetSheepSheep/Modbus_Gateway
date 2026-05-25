#ifndef __DRIVER_ADC_H
#define __DRIVER_ADC_H

#include <stdint.h>

void ADCDriverInit(void);
uint16_t ADCDriverRead(uint32_t channel);

#endif /* __DRIVER_ADC_H */
