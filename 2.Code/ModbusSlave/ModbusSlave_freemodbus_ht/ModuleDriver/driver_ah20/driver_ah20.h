#ifndef __DRIVER_AH20_H
#define __DRIVER_AH20_H
#include <stdint.h>

void AHT20DriverInitial(void);
void AHT20DriverTask(void);
void AHT20StartMeasure(void);
uint8_t AHT20IsReady(void);
uint8_t AHT20ReadTempHumi(float *temp, float *him);
void AHT20TempHimGetData(float *temp, float *him);
uint16_t AHT20GetTemp(void);
uint16_t AHT20GetHumi(void);

#endif /* __DRIVER_AH20_H */
