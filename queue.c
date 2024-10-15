#include <stdlib.h>

#include "queue.h"

Queue* queueCreate(unsigned capacity) {
    Queue* queue = (Queue*)malloc(sizeof(Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;
    queue->array = (Message*)malloc(queue->capacity * sizeof(Message));
    return queue;
}

int queueIsEmpty(Queue* queue) {
    return (queue->size == 0);
}

Message queueTop(Queue* queue) {
    return queue->array[queue->front];
}

int queueAdd(Queue* queue, Message item) {
    if (queue->size == queue->capacity) {
        return -1;
    }
    queue->rear = (queue->rear + 1) % queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
    return 0;
}

Message queueRemove(Queue* queue) {
    Message item = queue->array[queue->front];
    queue->front = (queue->front + 1) % queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

void queueDestroy(Queue* queue) {
    free(queue->array);
    free(queue);
}