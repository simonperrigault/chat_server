#define MAX_SIZE_MESSAGE 100
#define MAX_SIZE_NAME 20

typedef struct Message {
    char name[MAX_SIZE_NAME];
    char buf[MAX_SIZE_MESSAGE];
} Message;

typedef struct Queue {
    int front, rear, size;
    unsigned capacity;
    Message* array;
} Queue;

Queue* queueCreate(unsigned capacity);
int queueIsEmpty(Queue* queue);
Message queueTop(Queue* queue);
int queueAdd(Queue* queue, Message item);
Message queueRemove(Queue* queue);
void queueDestroy(Queue* queue);