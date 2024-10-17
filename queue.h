#define MAX_SIZE_MESSAGE 100
#define MAX_SIZE_NAME 20

// typedef struct Message {
//     char name[MAX_SIZE_NAME];
//     char buf[MAX_SIZE_MESSAGE];
// } Message;

typedef struct Queue {
    int front, rear, size;
    unsigned capacity;
    char** array;
} Queue;

Queue* queueCreate(unsigned capacity);
int queueIsEmpty(Queue* queue);
char* queueTop(Queue* queue);
int queueAdd(Queue* queue, char* item);
char* queueRemove(Queue* queue);
void queueDestroy(Queue* queue);