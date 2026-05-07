#include "ring_buffer.h"

void RingBuffer_Init(RingBuffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

// 入队（在串口中断中调用）
bool RingBuffer_Push(RingBuffer_t *rb, uint8_t data) {
    if (rb->count >= RING_BUFFER_SIZE) return false; // 缓冲区满
    rb->buffer[rb->tail] = data;
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    rb->count++;
    return true;
}

// 出队（在主循环中调用）
bool RingBuffer_Pop(RingBuffer_t *rb, uint8_t *data) {
    if (rb->count == 0) return false; // 缓冲区空
    *data = rb->buffer[rb->head];
    rb->head = (rb->head + 1) % RING_BUFFER_SIZE;
    
    // 关闭中断保护 count 变量，防止主循环和中断同时修改发生冲突
    __disable_irq(); 
    rb->count--;
    __enable_irq();
    
    return true;
}

uint16_t RingBuffer_GetCount(RingBuffer_t *rb) {
    return rb->count;
}
