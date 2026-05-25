#include "port_timer.h"
#include "modbus_rtu.h"
#include "config.h"

#define MODBUS_TIMER              TIMER2
#define MODBUS_TIMER_RCU          RCU_TIMER2
#define MODBUS_TIMER_IRQ          TIMER2_IRQn
#define MODBUS_TIMER_IRQ_PREEMPT_PRIO  1U
#define MODBUS_TIMER_IRQ_SUB_PRIO      1U
#define MODBUS_TIMER_CLK_HZ       120000000UL
#define MODBUS_TIMER_TICK_HZ      20000UL

void ModbusMasterPortTimerInit(USHORT usTim1Timerout50us)
{
    timer_parameter_struct timer_initpara;
    uint32_t prescaler;

    if(usTim1Timerout50us == 0)
    {
        usTim1Timerout50us = 1;
    }

    prescaler = (MODBUS_TIMER_CLK_HZ / MODBUS_TIMER_TICK_HZ) - 1UL;

    rcu_periph_clock_enable(MODBUS_TIMER_RCU);
    timer_deinit(MODBUS_TIMER);
    timer_struct_para_init(&timer_initpara);

    timer_initpara.prescaler = (uint16_t)prescaler;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.period = usTim1Timerout50us - 1U;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;

    timer_init(MODBUS_TIMER, &timer_initpara);
    timer_interrupt_enable(MODBUS_TIMER, TIMER_INT_UP);
    nvic_irq_enable(MODBUS_TIMER_IRQ,
                    MODBUS_TIMER_IRQ_PREEMPT_PRIO,
                    MODBUS_TIMER_IRQ_SUB_PRIO);
    ModbusMasterPortTimerDisable();
}

void ModbusMasterPortTimerEnable(void)
{
    timer_counter_value_config(MODBUS_TIMER, 0);
    timer_interrupt_flag_clear(MODBUS_TIMER, TIMER_INT_FLAG_UP);
    timer_enable(MODBUS_TIMER);
}

void ModbusMasterPortTimerDisable(void)
{
    timer_disable(MODBUS_TIMER);
    timer_counter_value_config(MODBUS_TIMER, 0);
    timer_interrupt_flag_clear(MODBUS_TIMER, TIMER_INT_FLAG_UP);
}

void TIMER2_IRQHandler(void)
{
    if(timer_interrupt_flag_get(MODBUS_TIMER, TIMER_INT_FLAG_UP) != RESET)
    {
        timer_interrupt_flag_clear(MODBUS_TIMER, TIMER_INT_FLAG_UP);
        timer_counter_value_config(MODBUS_TIMER, 0);
        ModbusMasterTimerExpiredISR();
    }
}
