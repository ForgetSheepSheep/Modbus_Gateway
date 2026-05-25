#include "port_serial.h"
#include "modbus_rtu.h"
#include "config.h"

#define MODBUS_USART       USART1
#define MODBUS_USART_RCU   RCU_USART1
#define MODBUS_USART_IRQ   USART1_IRQn
#define MODBUS_USART_IRQ_PREEMPT_PRIO  1U
#define MODBUS_USART_IRQ_SUB_PRIO      0U

#define MODBUS_GPIO_RCU    RCU_GPIOA
#define MODBUS_GPIO_PORT   GPIOA
#define MODBUS_TX_PIN      GPIO_PIN_2
#define MODBUS_RX_PIN      GPIO_PIN_3

#define RS485_DIR_RCU      RCU_GPIOC
#define RS485_DIR_PORT     GPIOC
#define RS485_DIR_PIN      GPIO_PIN_5

#define RS485_TX_MODE()    gpio_bit_set(RS485_DIR_PORT, RS485_DIR_PIN)
#define RS485_RX_MODE()    gpio_bit_reset(RS485_DIR_PORT, RS485_DIR_PIN)

static void RS485GpioInit(void)
{
    rcu_periph_clock_enable(RS485_DIR_RCU);
    gpio_init(RS485_DIR_PORT, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, RS485_DIR_PIN);
    RS485_RX_MODE();
}

static void UartInit(uint32_t baudRate)
{
    rcu_periph_clock_enable(MODBUS_GPIO_RCU);
    rcu_periph_clock_enable(MODBUS_USART_RCU);

    gpio_init(MODBUS_GPIO_PORT, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, MODBUS_TX_PIN);
    gpio_init(MODBUS_GPIO_PORT, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, MODBUS_RX_PIN);

    usart_deinit(MODBUS_USART);
    usart_baudrate_set(MODBUS_USART, baudRate);
    usart_word_length_set(MODBUS_USART, USART_WL_8BIT);
    usart_stop_bit_set(MODBUS_USART, USART_STB_1BIT);
    usart_parity_config(MODBUS_USART, USART_PM_NONE);
    usart_hardware_flow_rts_config(MODBUS_USART, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(MODBUS_USART, USART_CTS_DISABLE);
    usart_receive_config(MODBUS_USART, USART_RECEIVE_ENABLE);
    usart_transmit_config(MODBUS_USART, USART_TRANSMIT_ENABLE);

    nvic_irq_enable(MODBUS_USART_IRQ,
                    MODBUS_USART_IRQ_PREEMPT_PRIO,
                    MODBUS_USART_IRQ_SUB_PRIO);
    usart_enable(MODBUS_USART);
}

void ModbusMasterPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
    if(xRxEnable == TRUE)
    {
        RS485_RX_MODE();
        usart_interrupt_enable(MODBUS_USART, USART_INT_RBNE);
    }
    else
    {
        usart_interrupt_disable(MODBUS_USART, USART_INT_RBNE);
    }

    if(xTxEnable == TRUE)
    {
        RS485_TX_MODE();
    }
    else
    {
        RS485_RX_MODE();
    }
}

void ModbusMasterSerialInit(UCHAR port, ULONG baudRate, UCHAR dataBits, UCHAR parity)
{
    (void)port;
    (void)dataBits;
    (void)parity;

    RS485GpioInit();
    UartInit(baudRate);

    RS485_RX_MODE();
    usart_interrupt_disable(MODBUS_USART, USART_INT_RBNE);
}

void ModbusMasterPortSerialGetByte(CHAR *Byte)
{
    if(Byte == NULL)
    {
        return;
    }

    *Byte = (CHAR)(usart_data_receive(MODBUS_USART) & 0xFF);
}

void ModbusMasterSendFrame(UCHAR *frame, USHORT len)
{
    USHORT i;

    if(frame == NULL || len == 0)
    {
        return;
    }

    RS485_TX_MODE();

    for(i = 0; i < len; i++)
    {
        while(RESET == usart_flag_get(MODBUS_USART, USART_FLAG_TBE));
        usart_data_transmit(MODBUS_USART, frame[i]);
    }

    while(RESET == usart_flag_get(MODBUS_USART, USART_FLAG_TC));

    RS485_RX_MODE();
    printf("Modbus sent %d bytes\r\n", len);
}

void USART1_IRQHandler(void)
{
    if(usart_interrupt_flag_get(MODBUS_USART, USART_INT_FLAG_RBNE) != RESET)
    {
        ModbusMasterRxISR();
    }
}
