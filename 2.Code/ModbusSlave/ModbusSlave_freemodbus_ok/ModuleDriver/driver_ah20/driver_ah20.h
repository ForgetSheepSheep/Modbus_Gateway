#ifndef __DRIVER_AH20_H
#define __DRIVER_AH20_H
#include <stdint.h>

void AHT20DriverInitial(void);
void AHT20StartMeasure(void);
uint8_t AHT20IsReady(void);
void AHT20TempHimGetData(float *temp, float *him);

#endif /* __DRIVER_AH20_H */
