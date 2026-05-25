#ifndef __DRIVER_KEY_H
#define __DRIVER_KEY_H

#include "config.h"


/************************************************************
* @brief 按键编号枚举
* @note  用来区分当前是哪一个按键
************************************************************/
typedef enum
{
    KEY_ID_1 = 0,        /* 按键1 */
    KEY_ID_2,            /* 按键2 */
    KEY_ID_3,            /* 按键3 */
    KEY_ID_4,            /* 按键4 */
    KEY_ID_MAX,          /* 按键最大数量 */

} KeyId_t;


/************************************************************
* @brief 按键事件枚举
* @note  用来表示按键扫描后产生的事件
************************************************************/
typedef enum
{
    KEY_EVENT_NONE = 0,          /* 无按键事件 */
    KEY_EVENT_SHORT_PRESS,       /* 短按事件 */
    KEY_EVENT_DOUBLE_PRESS,      /* 双击事件 */
    KEY_EVENT_LONG_PRESS,        /* 长按事件 */

} KeyEvent_t;


/************************************************************
* @brief 按键事件结果结构体
* @note  用来告诉应用层：哪个按键产生了什么事件
************************************************************/
typedef struct
{
    KeyId_t key_id;              /* 按键编号 */
    KeyEvent_t key_event;        /* 按键事件 */

} KeyValue_t;


void DrvKeyInit(void);
void DrvKeyScan(void);
KeyValue_t DrvKeyReadValue(void);

#endif /* __DRIVER_KEY_H */
