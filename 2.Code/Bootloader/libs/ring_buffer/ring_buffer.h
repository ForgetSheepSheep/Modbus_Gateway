#ifndef  __RING_BUFFER_H
#define  __RING_BUFFER_H

/* 基本C库文件 */
#include <stdio.h>      
#include <stdint.h>     
#include <string.h>    

#define ESUCCESS (0U)
#define EFAIL    (1U)


typedef struct RingBuffer
{
    uint8_t *pbuffer;           /* 指向实际数据缓冲区的首地址，由外部数组提供 */

    volatile uint16_t buf_size; /* 环形缓冲区总大小，单位为字节 */

    volatile uint16_t pw;       /* 写指针位置，表示下一次数据写入的位置 */

    volatile uint16_t pr;       /* 读指针位置，表示下一次数据读取的位置 */

} RingBuffer_t, *pRingBuffer_t;


/************************************************************
* @brief 环形缓冲区初始化函数
* @param pdst_buf 环形缓冲区管理结构体指针
* @param buffer   外部提供的实际数据存储数组
* @param buf_size 缓冲区大小，单位为字节
* @return 0 表示初始化成功，其他值表示初始化失败
* @note  初始化时一般会清空读写指针，使读指针和写指针都从0开始
************************************************************/
uint8_t RingBufferInit(pRingBuffer_t pdst_buf, uint8_t *buffer, uint16_t buf_size);


/************************************************************
* @brief 向环形缓冲区写入1字节数据
* @param pdst_buf 环形缓冲区管理结构体指针
* @param byte     需要写入环形缓冲区的1字节数据
* @return 0 表示写入成功，其他值表示写入失败
* @note  正常情况下，写入成功后写指针pw会向后移动
*        如果缓冲区已满，则写入失败
************************************************************/
uint8_t RingBufferWrite(pRingBuffer_t pdst_buf, uint8_t byte);


/************************************************************
* @brief 从环形缓冲区读取1字节数据
* @param pdst_buf 环形缓冲区管理结构体指针
* @param byte     用来保存读取结果的变量地址
* @return 0 表示读取成功，其他值表示读取失败
* @note  正常情况下，读取成功后读指针pr会向后移动
*        如果缓冲区为空，则读取失败
************************************************************/
uint8_t RingBufferRead(pRingBuffer_t pdst_buf, uint8_t *byte);


#endif /* __RING_BUFFER_H */
