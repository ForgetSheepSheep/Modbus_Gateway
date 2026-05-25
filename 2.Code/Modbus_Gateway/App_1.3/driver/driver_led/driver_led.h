#ifndef __DRIVER_LED_H
#define __DRIVER_LED_H

#include "config.h"


/************************************************************
* @brief LED编号枚举
* @note  LED_ID_1 对应LED配置表中的第0个LED
************************************************************/
typedef enum
{
    LED_ID_1 = 0,          /* LED1编号 */
    LED_ID_2,              /* LED2编号 */
    LED_ID_3,              /* LED3编号 */

    LED_ID_MAX,            /* LED数量最大值，也用于越界判断 */

} LedId_e;


/************************************************************
* @brief LED驱动初始化
* @param 无
* @return 无
************************************************************/
void DrvLedInit(void);


/************************************************************
* @brief 控制指定LED亮灭
* @param led_id LED编号，取值参考LedId_e
* @param led_status LED状态，1输出高电平，0输出低电平
* @return 无
************************************************************/
void DrvLedCtrl(uint8_t led_id, uint8_t led_status);


#endif /* __DRIVER_LED_H */

