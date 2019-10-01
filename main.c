#include <stdio.h>
#include "coco.h"

coco_box_t box;

void hello(coco_msg_t msg) {
    printf("hello coco: %d\n", coco_msg_unwrap(msg, int));
    coco_send(box, 1);
    printf("goodbye coco\n");
}

int main() {
    coco_context_start();
    box = coco_box_create(0);
    coco_t co;
    coco_run(&co, hello, 12, 0);
    coco_recv(box, 0);
    printf("context end\n");
    coco_context_end();
}