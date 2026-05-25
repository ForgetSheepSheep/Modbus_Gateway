#include "./driver_uart/driver_uart.h"
#include "./ring_buffer/ring_buffer.h"
#include "./driver_4g_uart/driver_4g_uart.h"


/************************************************************
* 宏定义区
************************************************************/
#define UART_RX_DMA_BUF_SIZE        256     /* 每个串口DMA单次接收缓冲区大小 */
#define UART_RING_BUF_SIZE         1024     /* 每个串口环形缓冲区大小 */


/************************************************************
* 结构体定义区
************************************************************/
typedef struct
{
    rcu_periph_enum uart_rcu;       /* 串口外设时钟 */
    uint32_t uart_num;              /* 串口外设编号 */
    uint8_t irq;                    /* 串口中断号 */

    rcu_periph_enum gpio_rcu;       /* GPIO端口时钟 */
    uint32_t gpio_port;             /* GPIO端口 */
    uint32_t tx_pin;                /* TX引脚 */
    uint32_t rx_pin;                /* RX引脚 */

} UARTConfig_t;


typedef struct
{
    uint32_t dma_periph;            /* DMA外设编号 */
    dma_channel_enum dma_ch;        /* DMA通道 */

    uint8_t *rx_dma_buf;            /* DMA接收缓冲区 */
    uint16_t rx_dma_buf_size;       /* DMA接收缓冲区大小 */

} UARTDmaConfig_t;


/************************************************************
* 静态函数声明区
************************************************************/
static void DrvUARTGpioConfig(uint8_t uart_id);
static void DrvUARTPeripheralConfig(uint8_t uart_id, uint32_t baudrate);
static void DrvUARTDmaInit(uint8_t uart_id);
static void DrvUARTDmaRxRestart(uint8_t uart_id);
static void DrvUARTSendByte(uint8_t uart_id, uint8_t data);
static void DrvUARTIdleHandle(uint8_t uart_id);


/************************************************************
* 静态变量定义区
************************************************************/
static UARTConfig_t g_uart_tables[] =
{
    /* USART0：PA9=TX，PA10=RX，用于printf调试 */
    {RCU_USART0, USART0, USART0_IRQn, RCU_GPIOA, GPIOA, GPIO_PIN_9,  GPIO_PIN_10},

    /* USART2：PB10=TX，PB11=RX，用于连接串口LoRa模块 */
    {RCU_USART2, USART2, USART2_IRQn, RCU_GPIOB, GPIOB, GPIO_PIN_10, GPIO_PIN_11},
};

#define UART_NUM_MAX    (sizeof(g_uart_tables) / sizeof(g_uart_tables[0]))


/* USART0 DMA接收缓冲区 */
static uint8_t g_uart0_dma_rx_buf[UART_RX_DMA_BUF_SIZE];
static uint8_t g_uart0_ring_data[UART_RING_BUF_SIZE];
static RingBuffer_t g_uart0_ring_buf;


/* USART2 DMA接收缓冲区 */
static uint8_t g_uart2_dma_rx_buf[UART_RX_DMA_BUF_SIZE];
static uint8_t g_uart2_ring_data[UART_RING_BUF_SIZE];
static RingBuffer_t g_uart2_ring_buf;


/* 环形缓冲区表 */
static RingBuffer_t *g_uart_ring_tables[] =
{
    &g_uart0_ring_buf,
    &g_uart2_ring_buf,
};


/* DMA配置表 */
static UARTDmaConfig_t g_uart_dma_tables[] =
{
    /* USART0_RX：DMA0 CH4 */
    {DMA0, DMA_CH4, g_uart0_dma_rx_buf, UART_RX_DMA_BUF_SIZE},

    /* USART2_RX：DMA0 CH2 */
    {DMA0, DMA_CH2, g_uart2_dma_rx_buf, UART_RX_DMA_BUF_SIZE},
};


/************************************************************
* @brief 串口驱动初始化
* @param baudrate 串口波特率
* @return 无
* @note  当前初始化USART0和USART2，USART0用于调试，USART2用于LoRa
************************************************************/
void DrvUARTInit(uint32_t baudrate)
{
    uint8_t i = 0;

    RingBufferInit(&g_uart0_ring_buf, g_uart0_ring_data, UART_RING_BUF_SIZE);
    RingBufferInit(&g_uart2_ring_buf, g_uart2_ring_data, UART_RING_BUF_SIZE);

    for(i = 0; i < UART_ID_4G; i++)
    {
        DrvUARTGpioConfig(i);
        DrvUARTPeripheralConfig(i, baudrate);
        DrvUARTDmaInit(i);

        usart_enable(g_uart_tables[i].uart_num);
    }
}


/************************************************************
* @brief 串口发送指定长度数据
* @param uart_id 串口编号，0表示USART0，1表示USART2
* @param pdata 数据缓冲区指针
* @param len 数据长度
* @return 无
************************************************************/
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


/************************************************************
* @brief 串口发送字符串
* @param uart_id 串口编号，0表示USART0，1表示USART2
* @param str 字符串指针
* @return 无
************************************************************/
void DrvUARTSendString(uint8_t uart_id, char *str)
{

    if(uart_id >= UART_NUM_MAX)
    {
        printf("[UART] send invalid id=%u\r\n", uart_id);
        return;
    }

    if(str == 0)
    {
        printf("[UART%u] send null\r\n", uart_id);
        return;
    }

    while(*str != '\0')
    {
        DrvUARTSendByte(uart_id, (uint8_t)(*str));
        str++;
    }
}


/************************************************************
* @brief 从指定串口环形缓冲区读取1字节数据
* @param uart_id 串口编号，0表示USART0，1表示USART2
* @param pdata 读取到的数据保存地址
* @return 1表示读取成功，0表示没有数据或参数错误
************************************************************/

uint8_t DrvUARTReadByte(uint8_t uart_id, uint8_t *pdata)
{
    if(uart_id >= UART_NUM_MAX)
    {
        return 0;
    }

    if(pdata == 0)
    {
        return 0;
    }

    return RingBufferRead(g_uart_ring_tables[uart_id], pdata);
}


/************************************************************
* 内部辅助函数区
************************************************************/

/************************************************************
* @brief 串口GPIO初始化
* @param uart_id 串口编号
* @return 无
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


/************************************************************
* @brief 串口外设初始化
* @param uart_id 串口编号
* @param baudrate 波特率
* @return 无
************************************************************/
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


/************************************************************
* @brief 串口DMA接收初始化
* @param uart_id 串口编号
* @return 无
************************************************************/
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


/************************************************************
* @brief 重新启动指定串口DMA接收
* @param uart_id 串口编号
* @return 无
************************************************************/
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


/************************************************************
* @brief 串口发送1字节数据
* @param uart_id 串口编号
* @param data 要发送的数据
* @return 无
************************************************************/
static void DrvUARTSendByte(uint8_t uart_id, uint8_t data)
{
    uint32_t timeout = 200000U;

    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    while(RESET == usart_flag_get(g_uart_tables[uart_id].uart_num, USART_FLAG_TBE))
    {
        if(timeout-- == 0U)
        {
            printf("[UART%u] tx wait TBE timeout\r\n", uart_id);
            return;
        }
    }

    usart_data_transmit(g_uart_tables[uart_id].uart_num, data);

    timeout = 200000U;
    while(RESET == usart_flag_get(g_uart_tables[uart_id].uart_num, USART_FLAG_TC))
    {
        if(timeout-- == 0U)
        {
            printf("[UART%u] tx wait TC timeout\r\n", uart_id);
            return;
        }
    }
}


/************************************************************
* @brief 串口空闲中断统一处理函数
* @param uart_id 串口编号
* @return 无
* @note  DMA收到一包数据后，空闲中断触发，把DMA缓冲区数据搬到环形缓冲区
************************************************************/
static void DrvUARTIdleHandle(uint8_t uart_id)
{
    uint16_t i = 0;
    uint16_t remain_len = 0;
    uint16_t recv_len = 0;
    uint32_t uart_num = 0;

    if(uart_id >= UART_NUM_MAX)
    {
        return;
    }

    uart_num = g_uart_tables[uart_id].uart_num;

    if(RESET != usart_interrupt_flag_get(uart_num, USART_INT_FLAG_IDLE))
    {
        usart_data_receive(uart_num);

        dma_channel_disable(g_uart_dma_tables[uart_id].dma_periph,
                            g_uart_dma_tables[uart_id].dma_ch);

        remain_len = dma_transfer_number_get(g_uart_dma_tables[uart_id].dma_periph,
                                              g_uart_dma_tables[uart_id].dma_ch);

        recv_len = g_uart_dma_tables[uart_id].rx_dma_buf_size - remain_len;

        if(recv_len > 0)
        {
            for(i = 0; i < recv_len; i++)
            {
                RingBufferWrite(g_uart_ring_tables[uart_id],
                                g_uart_dma_tables[uart_id].rx_dma_buf[i]);
            }
        }

        DrvUARTDmaRxRestart(uart_id);
    }
}


/************************************************************
* 中断服务函数区
************************************************************/

/************************************************************
* @brief USART0中断服务函数
* @param 无
* @return 无
************************************************************/
void USART0_IRQHandler(void)
{
    DrvUARTIdleHandle(0);
}


/************************************************************
* @brief USART2中断服务函数
* @param 无
* @return 无
************************************************************/
void USART2_IRQHandler(void)
{
    Drv4GUARTIRQHandler();
}


/************************************************************
* printf重定向区
************************************************************/
int fputc(int ch, FILE *f)
{
    DrvUARTSendByte(0, (uint8_t)ch);

    return ch;
}
