#include "./driver_ah20/driver_ah20.h"

void AHT20DriverInitial(void)
{
}

void AHT20DriverTask(void)
{
}

void AHT20StartMeasure(void)
{
}

uint8_t AHT20IsReady(void)
{
    return 0;
}

void AHT20TempHimGetData(float *temp, float *him)
{
    if (temp != 0)
    {
        *temp = 0.0f;
    }
    if (him != 0)
    {
        *him = 0.0f;
    }
}

uint16_t AHT20GetTemp(void)
{
    return 0;
}

uint16_t AHT20GetHumi(void)
{
    return 0;
}
