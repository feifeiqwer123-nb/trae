#ifndef __RING_BUFFER_H
#define __RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

#define RING_BUFFER_SIZE  1024  // 缓冲区大小，建议设置大一点以防主循环卡顿

typedef struct {
    uint8_t  buffer[RING_BUFFER_SIZE];
    uint16_t head;
    uint16_t tail;
    uint16_t count;
} RingBuffer_t;

void RingBuffer_Init(RingBuffer_t *rb);
bool RingBuffer_Push(RingBuffer_t *rb, uint8_t data);
bool RingBuffer_Pop(RingBuffer_t *rb, uint8_t *data);
uint16_t RingBuffer_GetCount(RingBuffer_t *rb);

#endif
