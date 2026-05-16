#include "./ring_buffer/ring_buffer.h"

/************************************************************
* @brief 环形缓冲区初始化函数
* @param pdst_buf 环形缓冲区管理结构体指针
* @param buffer   外部提供的实际数据存储数组
* @param buf_size 缓冲区大小，单位为字节
* @return ESUCCESS 表示初始化成功，EFAIL 表示初始化失败
* @note  初始化时会绑定外部缓冲区数组，并清空读写指针
*        pr 表示读指针，指向下一次要读取的位置
*        pw 表示写指针，指向下一次要写入的位置
*        初始化完成后 pr 和 pw 都为 0，表示当前缓冲区为空
************************************************************/
uint8_t RingBufferInit(pRingBuffer_t pdst_buf, uint8_t *buffer, uint16_t buf_size)
{
    if(pdst_buf == NULL) return EFAIL;      /* 判断环形缓冲区管理结构体指针是否为空，如果为空则初始化失败 */
    if(buffer == NULL)   return EFAIL;      /* 判断外部提供的数据缓冲区是否为空，如果为空则初始化失败 */
    if(buf_size < 2)     return EFAIL;      /* 判断缓冲区大小是否小于2，因为该环形缓冲区会空出1个位置用于区分空和满 */
    
    pdst_buf->buf_size = buf_size;          /* 保存环形缓冲区的总大小，后续读写指针回卷时需要使用 */
    pdst_buf->pbuffer  = buffer;            /* 保存外部数据缓冲区的首地址，后续数据实际存放在该数组中 */
    pdst_buf->pr = 0U;                      /* 读指针清零，表示下一次从下标0开始读取 */
    pdst_buf->pw = 0U;                      /* 写指针清零，表示下一次从下标0开始写入 */
    
    return ESUCCESS;                        /* 初始化成功，返回成功状态 */
}


/************************************************************
* @brief 向环形缓冲区写入1字节数据
* @param pdst_buf 环形缓冲区管理结构体指针
* @param byte     需要写入环形缓冲区的数据
* @return ESUCCESS 表示函数执行完成，EFAIL 表示参数异常
* @note  写入前会先计算写指针的下一个位置 w_next
*        如果 w_next 不等于读指针 pr，说明缓冲区未满，可以写入
*        如果 w_next 等于读指针 pr，说明缓冲区已满，本次不会写入
*        该环形缓冲区采用“空出1个字节”的方式区分空和满
*        pr == pw 表示缓冲区为空
*        (pw + 1) % buf_size == pr 表示缓冲区已满
************************************************************/
uint8_t RingBufferWrite(pRingBuffer_t pdst_buf, uint8_t byte)
{
    if(pdst_buf == NULL || pdst_buf->pbuffer == NULL) return EFAIL;    /* 判断环形缓冲区结构体或实际数据缓冲区是否为空，如果为空则写入失败 */
    
    uint16_t w_next = (pdst_buf->pw + 1) % pdst_buf->buf_size;         /* 计算写指针的下一个位置，如果到达数组末尾，则通过取余回到0，用于实现回卷 */
    
    if(w_next == pdst_buf->pr)                                         /* 如果写指针的下一个位置等于读指针，说明缓冲区已经写满 */
    {
        return EFAIL;                                                  /* 缓冲区已满，本次写入失败 */
    }

    pdst_buf->pbuffer[pdst_buf->pw] = byte;                            /* 将数据写入当前写指针 pw 所指向的位置 */
    pdst_buf->pw = w_next;                                             /* 更新写指针，让写指针移动到下一个可写位置 */
    
    return ESUCCESS;                                                   /* 写入成功，返回成功状态 */
}

/************************************************************
* @brief 从环形缓冲区读取1字节数据
* @param pdst_buf 环形缓冲区管理结构体指针
* @param byte     用来保存读取结果的变量地址
* @return ESUCCESS 表示读取成功，EFAIL 表示读取失败
* @note  读取前会先判断缓冲区是否为空
*        如果 pr == pw，说明读指针和写指针重合，缓冲区为空
*        如果 pr != pw，说明缓冲区中有数据可以读取
*        读取成功后，读指针 pr 会移动到下一个位置
*        当读指针到达数组末尾后，会通过取余重新回到0
************************************************************/
uint8_t RingBufferRead(pRingBuffer_t pdst_buf, uint8_t *byte)
{
    if(pdst_buf == NULL || byte == NULL || pdst_buf->pbuffer == NULL) return EFAIL;    /* 判断结构体指针、数据保存地址、实际缓冲区地址是否为空，如果为空则读取失败 */
    if(pdst_buf->pr == pdst_buf->pw) return EFAIL;                                    /* 判断缓冲区是否为空，如果读指针等于写指针，说明没有数据可以读取 */
    
    *byte = pdst_buf->pbuffer[pdst_buf->pr];                                           /* 读取当前读指针 pr 所指向位置的数据，并通过 byte 指针带出函数 */
    
    pdst_buf->pr = (pdst_buf->pr + 1) % pdst_buf->buf_size;                            /* 更新读指针，如果到达数组末尾，则通过取余回到0，用于实现回卷 */
    return ESUCCESS;                                                                   /* 读取成功，返回成功状态 */
}
