#include "./driver_uart/driver_uart.h"
#include "./ring_buffer/ring_buffer.h"


/************************************************************
* 宏定义区
************************************************************/
#define UART0_RX_DMA_BUF_SIZE     256
#define UART0_RING_BUF_SIZE       512


/************************************************************
* 结构体定义区
************************************************************/
typedef struct
{
    rcu_periph_enum uart_rcu;
    uint32_t uart_num;
    uint8_t irq;

    rcu_periph_enum gpio_rcu;
    uint32_t gpio_port;
    uint32_t tx_pin;
    uint32_t rx_pin;

} UARTConfig_t;


typedef struct
{
    uint32_t dma_periph;
    dma_channel_enum dma_ch;

    uint8_t *rx_dma_buf;
    uint16_t rx_dma_buf_size;

} UARTDmaConfig_t;


/************************************************************
* 静态函数声明区
************************************************************/
static void DrvUARTGpioConfig(uint8_t uart_id);
static void DrvUARTPeripheralConfig(uint8_t uart_id, uint32_t baudrate);
static void DrvUARTDmaInit(uint8_t uart_id);
static void DrvUARTDmaRxRestart(uint8_t uart_id);
static void DrvUARTSendByte(uint8_t uart_id, uint8_t data);


/************************************************************
* 静态变量定义区
************************************************************/
static UARTConfig_t g_uart_tables[] =
{
    {RCU_USART0, USART0, USART0_IRQn, RCU_GPIOA, GPIOA, GPIO_PIN_9, GPIO_PIN_10},
};

#define UART_NUM_MAX    (sizeof(g_uart_tables) / sizeof(g_uart_tables[0]))

static uint8_t g_uart0_dma_rx_buf[UART0_RX_DMA_BUF_SIZE];
static uint8_t g_uart0_ring_data[UART0_RING_BUF_SIZE];

static RingBuffer_t g_uart0_ring_buf;

static UARTDmaConfig_t g_uart_dma_tables[] =
{
    {DMA0, DMA_CH4, g_uart0_dma_rx_buf, UART0_RX_DMA_BUF_SIZE},
};


/************************************************************
* 对外接口函数区
************************************************************/
void DrvUARTInit(uint32_t baudrate)
{
    uint8_t i = 0;

    RingBufferInit(&g_uart0_ring_buf, g_uart0_ring_data, UART0_RING_BUF_SIZE);

    for(i = 0; i < UART_NUM_MAX; i++)
    {
        DrvUARTGpioConfig(i);
        DrvUARTPeripheralConfig(i, baudrate);
        DrvUARTDmaInit(i);

        usart_enable(g_uart_tables[i].uart_num);
    }
}


void DrvUARTSendData(uint8_t uart_id, uint8_t *pdata, uint16_t len)
{
    uint16_t i = 0;

    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    if(pdata == 0)
    {
        return;
    }

    for(i = 0; i < len; i++)
    {
        DrvUARTSendByte(uart_id, pdata[i]);
    }
}


void DrvUARTSendString(uint8_t uart_id, char *str)
{
    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    if(str == 0)
    {
        return;
    }

    while(*str != '\0')
    {
        DrvUARTSendByte(uart_id, (uint8_t)(*str));
        str++;
    }
}


uint8_t DrvUARTReadByte(uint8_t *pdata)
{
    if(pdata == 0)
    {
        return 0;
    }

    return RingBufferRead(&g_uart0_ring_buf, pdata);
}


/************************************************************
* 内部辅助函数区
************************************************************/
static void DrvUARTGpioConfig(uint8_t uart_id)
{
    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    rcu_periph_clock_enable(g_uart_tables[uart_id].gpio_rcu);
    rcu_periph_clock_enable(g_uart_tables[uart_id].uart_rcu);

    gpio_init(g_uart_tables[uart_id].gpio_port,
              GPIO_MODE_AF_PP,
              GPIO_OSPEED_50MHZ,
              g_uart_tables[uart_id].tx_pin);

    gpio_init(g_uart_tables[uart_id].gpio_port,
              GPIO_MODE_IN_FLOATING,
              GPIO_OSPEED_50MHZ,
              g_uart_tables[uart_id].rx_pin);
}


static void DrvUARTPeripheralConfig(uint8_t uart_id, uint32_t baudrate)
{
    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    usart_deinit(g_uart_tables[uart_id].uart_num);

    usart_baudrate_set(g_uart_tables[uart_id].uart_num, baudrate);
    usart_word_length_set(g_uart_tables[uart_id].uart_num, USART_WL_8BIT);
    usart_stop_bit_set(g_uart_tables[uart_id].uart_num, USART_STB_1BIT);
    usart_parity_config(g_uart_tables[uart_id].uart_num, USART_PM_NONE);

    usart_hardware_flow_rts_config(g_uart_tables[uart_id].uart_num, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(g_uart_tables[uart_id].uart_num, USART_CTS_DISABLE);

    usart_receive_config(g_uart_tables[uart_id].uart_num, USART_RECEIVE_ENABLE);
    usart_transmit_config(g_uart_tables[uart_id].uart_num, USART_TRANSMIT_ENABLE);

    usart_interrupt_enable(g_uart_tables[uart_id].uart_num, USART_INT_IDLE);
    nvic_irq_enable(g_uart_tables[uart_id].irq, 2, 2);

    usart_dma_receive_config(g_uart_tables[uart_id].uart_num, USART_RECEIVE_DMA_ENABLE);
}


static void DrvUARTDmaInit(uint8_t uart_id)
{
    dma_parameter_struct dma_init_struct;

    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    rcu_periph_clock_enable(RCU_DMA0);

    dma_deinit(g_uart_dma_tables[uart_id].dma_periph,
               g_uart_dma_tables[uart_id].dma_ch);

    dma_struct_para_init(&dma_init_struct);

    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(g_uart_tables[uart_id].uart_num);
    dma_init_struct.memory_addr = (uint32_t)g_uart_dma_tables[uart_id].rx_dma_buf;
    dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_init_struct.number = g_uart_dma_tables[uart_id].rx_dma_buf_size;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;

    dma_init(g_uart_dma_tables[uart_id].dma_periph,
             g_uart_dma_tables[uart_id].dma_ch,
             &dma_init_struct);

    dma_circulation_disable(g_uart_dma_tables[uart_id].dma_periph,
                            g_uart_dma_tables[uart_id].dma_ch);

    dma_channel_enable(g_uart_dma_tables[uart_id].dma_periph,
                       g_uart_dma_tables[uart_id].dma_ch);
}


static void DrvUARTDmaRxRestart(uint8_t uart_id)
{
    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    dma_channel_disable(g_uart_dma_tables[uart_id].dma_periph,
                        g_uart_dma_tables[uart_id].dma_ch);

    dma_transfer_number_config(g_uart_dma_tables[uart_id].dma_periph,
                               g_uart_dma_tables[uart_id].dma_ch,
                               g_uart_dma_tables[uart_id].rx_dma_buf_size);

    dma_channel_enable(g_uart_dma_tables[uart_id].dma_periph,
                       g_uart_dma_tables[uart_id].dma_ch);
}


static void DrvUARTSendByte(uint8_t uart_id, uint8_t data)
{
    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    while(RESET == usart_flag_get(g_uart_tables[uart_id].uart_num, USART_FLAG_TBE));

    usart_data_transmit(g_uart_tables[uart_id].uart_num, data);

    while(RESET == usart_flag_get(g_uart_tables[uart_id].uart_num, USART_FLAG_TC));
}


/************************************************************
* 中断服务函数区
************************************************************/
void USART0_IRQHandler(void)
{
    uint16_t i = 0;
    uint16_t remain_len = 0;
    uint16_t recv_len = 0;

    if(RESET != usart_interrupt_flag_get(USART0, USART_INT_FLAG_IDLE))
    {
        usart_data_receive(USART0);

        dma_channel_disable(DMA0, DMA_CH4);

        remain_len = dma_transfer_number_get(DMA0, DMA_CH4);

        recv_len = UART0_RX_DMA_BUF_SIZE - remain_len;

        if(recv_len > 0)
        {
            for(i = 0; i < recv_len; i++)
            {
                RingBufferWrite(&g_uart0_ring_buf, g_uart0_dma_rx_buf[i]);
            }
        }

        DrvUARTDmaRxRestart(0);
    }
}


/************************************************************
* printf重定向区
************************************************************/
int fputc(int ch, FILE *f)
{
    DrvUARTSendByte(0, (uint8_t)ch);

    return ch;
}
