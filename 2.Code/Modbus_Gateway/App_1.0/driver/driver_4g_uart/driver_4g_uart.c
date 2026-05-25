#include "./driver_4g_uart/driver_4g_uart.h"
#include "./ring_buffer/ring_buffer.h"

#define DRV_4G_UART              USART2
#define DRV_4G_UART_RCU          RCU_USART2
#define DRV_4G_UART_IRQ          USART2_IRQn
#define DRV_4G_GPIO_RCU          RCU_GPIOB
#define DRV_4G_GPIO_PORT         GPIOB
#define DRV_4G_TX_PIN            GPIO_PIN_10
#define DRV_4G_RX_PIN            GPIO_PIN_11

#define DRV_4G_DMA               DMA0
#define DRV_4G_DMA_CH_RX         DMA_CH2
#define DRV_4G_DMA_BUF_SIZE      256U
#define DRV_4G_RING_BUF_SIZE     1024U
#define DRV_4G_TX_TIMEOUT        200000U

static uint8_t g_4g_dma_rx_buf[DRV_4G_DMA_BUF_SIZE];
static uint8_t g_4g_ring_data[DRV_4G_RING_BUF_SIZE];
static RingBuffer_t g_4g_ring_buf;

static void Drv4GUARTGpioInit(void);
static void Drv4GUARTPeripheralInit(uint32_t baudrate);
static void Drv4GUARTDmaInit(void);
static void Drv4GUARTDmaRxRestart(void);
static uint8_t Drv4GUARTSendByte(uint8_t data);

void Drv4GUARTInit(uint32_t baudrate)
{
    RingBufferInit(&g_4g_ring_buf, g_4g_ring_data, DRV_4G_RING_BUF_SIZE);

    Drv4GUARTGpioInit();
    Drv4GUARTPeripheralInit(baudrate);
    Drv4GUARTDmaInit();

    usart_enable(DRV_4G_UART);

}

uint8_t Drv4GUARTSendString(char *str)
{
    uint16_t len = 0;
    char *p = str;

    if(str == NULL)
    {
        printf("[4G-UART] send null\r\n");
        return EFAIL;
    }

    while(p[len] != '\0')
    {
        len++;
    }

    while(*str != '\0')
    {
        if(Drv4GUARTSendByte((uint8_t)(*str)) != ESUCCESS)
        {
            printf("[4G-UART] send failed\r\n");
            return EFAIL;
        }
        str++;
    }

    return ESUCCESS;
}

uint8_t Drv4GUARTReadByte(uint8_t *pdata)
{
    return RingBufferRead(&g_4g_ring_buf, pdata);
}

void Drv4GUARTIRQHandler(void)
{
    uint16_t i;
    uint16_t remain_len;
    uint16_t recv_len;

    if(RESET == usart_interrupt_flag_get(DRV_4G_UART, USART_INT_FLAG_IDLE))
    {
        return;
    }

    (void)usart_data_receive(DRV_4G_UART);

    dma_channel_disable(DRV_4G_DMA, DRV_4G_DMA_CH_RX);

    remain_len = dma_transfer_number_get(DRV_4G_DMA, DRV_4G_DMA_CH_RX);
    recv_len = DRV_4G_DMA_BUF_SIZE - remain_len;

    if(recv_len > 0U)
    {
        for(i = 0; i < recv_len; i++)
        {
            RingBufferWrite(&g_4g_ring_buf, g_4g_dma_rx_buf[i]);
        }
    }

    Drv4GUARTDmaRxRestart();
}

static void Drv4GUARTGpioInit(void)
{
    rcu_periph_clock_enable(DRV_4G_GPIO_RCU);
    rcu_periph_clock_enable(DRV_4G_UART_RCU);

    gpio_init(DRV_4G_GPIO_PORT,
              GPIO_MODE_AF_PP,
              GPIO_OSPEED_50MHZ,
              DRV_4G_TX_PIN);

    gpio_init(DRV_4G_GPIO_PORT,
              GPIO_MODE_IN_FLOATING,
              GPIO_OSPEED_50MHZ,
              DRV_4G_RX_PIN);
}

static void Drv4GUARTPeripheralInit(uint32_t baudrate)
{
    usart_deinit(DRV_4G_UART);
    usart_baudrate_set(DRV_4G_UART, baudrate);
    usart_word_length_set(DRV_4G_UART, USART_WL_8BIT);
    usart_stop_bit_set(DRV_4G_UART, USART_STB_1BIT);
    usart_parity_config(DRV_4G_UART, USART_PM_NONE);
    usart_hardware_flow_rts_config(DRV_4G_UART, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(DRV_4G_UART, USART_CTS_DISABLE);
    usart_receive_config(DRV_4G_UART, USART_RECEIVE_ENABLE);
    usart_transmit_config(DRV_4G_UART, USART_TRANSMIT_ENABLE);

    usart_interrupt_enable(DRV_4G_UART, USART_INT_IDLE);
    nvic_irq_enable(DRV_4G_UART_IRQ, 2, 2);

    usart_dma_receive_config(DRV_4G_UART, USART_RECEIVE_DMA_ENABLE);
}

static void Drv4GUARTDmaInit(void)
{
    dma_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(RCU_DMA0);

    dma_deinit(DRV_4G_DMA, DRV_4G_DMA_CH_RX);
    dma_struct_para_init(&dma_init_struct);

    dma_init_struct.periph_addr = (uint32_t)&USART_DATA(DRV_4G_UART);
    dma_init_struct.memory_addr = (uint32_t)g_4g_dma_rx_buf;
    dma_init_struct.direction = DMA_PERIPHERAL_TO_MEMORY;
    dma_init_struct.number = DRV_4G_DMA_BUF_SIZE;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_width = DMA_PERIPHERAL_WIDTH_8BIT;
    dma_init_struct.memory_width = DMA_MEMORY_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;

    dma_init(DRV_4G_DMA, DRV_4G_DMA_CH_RX, &dma_init_struct);
    dma_circulation_disable(DRV_4G_DMA, DRV_4G_DMA_CH_RX);
    dma_channel_enable(DRV_4G_DMA, DRV_4G_DMA_CH_RX);
}

static void Drv4GUARTDmaRxRestart(void)
{
    dma_channel_disable(DRV_4G_DMA, DRV_4G_DMA_CH_RX);
    dma_transfer_number_config(DRV_4G_DMA, DRV_4G_DMA_CH_RX, DRV_4G_DMA_BUF_SIZE);
    dma_channel_enable(DRV_4G_DMA, DRV_4G_DMA_CH_RX);
}

static uint8_t Drv4GUARTSendByte(uint8_t data)
{
    uint32_t timeout = DRV_4G_TX_TIMEOUT;

    while(RESET == usart_flag_get(DRV_4G_UART, USART_FLAG_TBE))
    {
        if(timeout-- == 0U)
        {
            printf("[4G-UART] wait TBE timeout\r\n");
            return EFAIL;
        }
    }

    usart_data_transmit(DRV_4G_UART, data);

    timeout = DRV_4G_TX_TIMEOUT;
    while(RESET == usart_flag_get(DRV_4G_UART, USART_FLAG_TC))
    {
        if(timeout-- == 0U)
        {
            printf("[4G-UART] wait TC timeout\r\n");
            return EFAIL;
        }
    }

    return ESUCCESS;
}
