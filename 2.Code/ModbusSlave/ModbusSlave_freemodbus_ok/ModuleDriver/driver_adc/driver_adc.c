#include "./driver_adc/driver_adc.h"
#include "stm32f0xx_hal.h"

extern ADC_HandleTypeDef hadc;

void ADCDriverInit(void)
{
    HAL_ADCEx_Calibration_Start(&hadc);
}

uint16_t ADCDriverRead(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
    sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
    {
        return 0;
    }

    HAL_ADC_Start(&hadc);
    if (HAL_ADC_PollForConversion(&hadc, 100) != HAL_OK)
    {
        return 0;
    }
    return (uint16_t)HAL_ADC_GetValue(&hadc);
}
