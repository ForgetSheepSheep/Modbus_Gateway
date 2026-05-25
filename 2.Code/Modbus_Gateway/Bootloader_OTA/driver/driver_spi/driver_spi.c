#include "driver_spi/driver_spi.h"



/************************************************************
* @brief 硬件SPI初始化
* @param 无
* @return 无
* @note  PB13=SCK，PB14=MISO，PB15=MOSI，PE2=CS
************************************************************/
void DrvSPIInit(void)
{
    spi_parameter_struct spi_init_struct;

    /* 1. 使能GPIOB时钟，因为PB13/PB14/PB15用于SPI */
    rcu_periph_clock_enable(RCU_GPIOB);

    /* 2. 使能GPIOE时钟，因为PE2用于Flash片选CS */
    rcu_periph_clock_enable(RCU_GPIOE);

    /* 3. 使能SPI1外设时钟 */
    rcu_periph_clock_enable(RCU_SPI1);

    /* 4. 配置PB13=SCK，PB15=MOSI，复用推挽输出 */
    gpio_init(GPIOB,
              GPIO_MODE_AF_PP,
              GPIO_OSPEED_50MHZ,
              GPIO_PIN_13 | GPIO_PIN_15);

    /* 5. 配置PB14=MISO，浮空输入 */
    gpio_init(GPIOB,
              GPIO_MODE_IN_FLOATING,
              GPIO_OSPEED_50MHZ,
              GPIO_PIN_14);

    /* 6. 配置PE2=CS，普通推挽输出 */
    gpio_init(GPIOE,
              GPIO_MODE_OUT_PP,
              GPIO_OSPEED_50MHZ,
              GPIO_PIN_2);

    /* 7. 默认不选中SPI Flash，CS拉高 */
    SPI_FLASH_CS_HIGH();

    /* 8. 复位SPI1外设 */
    spi_i2s_deinit(SPI1);

    /* 9. 将SPI参数结构体恢复默认值 */
    spi_struct_para_init(&spi_init_struct);

    /* 10. 配置SPI为全双工模式 */
    spi_init_struct.trans_mode = SPI_TRANSMODE_FULLDUPLEX;

    /* 11. 配置SPI为主机模式，MCU主动产生SCK */
    spi_init_struct.device_mode = SPI_MASTER;

    /* 12. 配置SPI数据帧为8位 */
    spi_init_struct.frame_size = SPI_FRAMESIZE_8BIT;

    /* 13. 配置SPI模式0：SCK空闲低电平，第1个边沿采样 */
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_1EDGE;

    /* 14. 使用软件NSS，不让硬件自动控制片选 */
    spi_init_struct.nss = SPI_NSS_SOFT;

    /* 15. SPI分频，先慢一点，稳定后再提高 */
    spi_init_struct.prescale = SPI_PSC_8;

    /* 16. SPI高位先发 */
    spi_init_struct.endian = SPI_ENDIAN_MSB;

    /* 17. 初始化SPI1 */
    spi_init(SPI1, &spi_init_struct);

    /* 18. 软件NSS拉高，防止主机模式异常 */
    spi_nss_internal_high(SPI1);

    /* 19. 使能SPI1 */
    spi_enable(SPI1);
}

/************************************************************
* @brief SPI发送并接收1字节
* @param tx_data 要发送的数据
* @return 接收到的数据
* @note  SPI全双工，发送1字节的同时会接收1字节
************************************************************/
uint8_t DrvSPIReadWriteByte(uint8_t tx_data)
{
    uint32_t timeout = 0xFFFF;

    /* 等待发送缓冲区为空 */
    while(RESET == spi_i2s_flag_get(SPI1, SPI_FLAG_TBE))
    {
        if(timeout-- == 0)
        {
            return 0xFF;
        }
    }

    /* 写入要发送的数据，SPI硬件会自动产生SCK并从MOSI发出 */
    spi_i2s_data_transmit(SPI1, tx_data);

    timeout = 0xFFFF;

    /* 等待接收缓冲区非空 */
    while(RESET == spi_i2s_flag_get(SPI1, SPI_FLAG_RBNE))
    {
        if(timeout-- == 0)
        {
            return 0xFF;
        }
    }

    /* 返回从MISO收到的数据 */
    return (uint8_t)spi_i2s_data_receive(SPI1);
}

