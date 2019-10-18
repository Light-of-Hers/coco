#include "coco.h"
#include <stdio.h>

#define N_COCO 10
#define N_ITEM 100

static __thread coco_box_t box;
static __thread int active;

void consumer(coco_msg_t msg) {
    int id = coco_msg_unwrap(msg, int);
    while (coco_recv(box, &msg) >= 0) {
        printf("[%d] consume %d\n", id, coco_msg_unwrap(msg, int));
    }
    active--;
}

void producer() {
    for (int i = 0; i < N_ITEM; ++i) {
        printf("produce %d\n", i);
        coco_send(box, i);
    }
}

int main() {
    coco_context_start();

    box = coco_box_create(0);

    coco_t co;
    coco_run(&co, consumer, 0, 0), active = 1;
    for (int i = 1; i < N_COCO; ++i)
        coco_run(0, consumer, i, co), active++;
    producer();

    coco_box_close(box);
    while (active)
        coco_yield();

    coco_context_end();
}