#include "./driver_systick/driver_systick.h"
#include "stm32f0xx_hal.h"

/************************************************************
 @brief  SysTick �����������������
************************************************************/
static volatile uint32_t s_tick_ms = 0;       // ϵͳ�������
static void (*s_tick_callback)(void) = NULL;  // �û��ص�����

/************************************************************
 @brief  SysTick ��ʼ��������1ms ����
 @param  none
 @return none
************************************************************/
void SysTickInit(void)
{
    // HAL �ײ��ʼ�� SysTick��1ms ����
    HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    // ���� SysTick �ж����ȼ� (低于 TIM3=0, USART1=1)
    HAL_NVIC_SetPriority(SysTick_IRQn, 2, 0);
}

/************************************************************
 @brief  ע�� SysTick �ص�����
 @param  cb �û��ص�����
 @return none
************************************************************/
void SysTickRegisterCallback(void (*cb)(void))
{
    s_tick_callback = cb;
}

/************************************************************
 @brief  ��ȡϵͳ������
 @return ��ǰϵͳ�������
************************************************************/
uint32_t SysTickGetTick(void)
{
    return s_tick_ms;
}

/************************************************************
 @brief  SysTick �жϷ�����
************************************************************/
void SysTick_Handler(void)
{
    s_tick_ms++;         // �Լ�����������

    // �����û��ص�����
    if(s_tick_callback)
        s_tick_callback();
}

