#include "coco.h"
#include <stdio.h>

#define NCOCO 10
#define NPING 10

coco_box_t box;

void ping_pong(coco_msg_t msg) {
    if (msg % 2 == 0) {
        while (coco_recv(box, &msg) >= 0) {
            printf("[%016lx] pong[%d]\n", coco_self(), coco_msg_unwrap(msg, int));
            coco_send(box, 0);
        }
    } else {
        for (int i = 0; i < NPING; ++i) {
            printf("[%016lx] ping[%d]\n", coco_self(), i);
            coco_send(box, coco_msg_wrap(i));
            coco_recv(box, 0);
        }
    }
}

int main() {
    coco_context_start();
    box = coco_box_create(0);
    coco_t co;
    coco_run(&co, &ping_pong, 0, 0);
    ping_pong(1);
    coco_box_close(box);
    coco_context_end();
}