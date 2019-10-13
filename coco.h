//
// Created by herlight on 2019/9/20.
//

#ifndef COCO_COCO_H
#define COCO_COCO_H

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>

typedef uintptr_t coco_t, coco_box_t;
typedef uint64_t coco_msg_t;
#define coco_msg_wrap(_x) ((coco_msg_t)(_x))
#define coco_msg_unwrap(_x, _t) ((_t)(_x))
typedef struct {
    coco_box_t box;
    coco_msg_t msg;
    int valid;
    int is_send;
} coco_way_t;
typedef void coco_routine_t(coco_msg_t arg);

void coco_context_start();

void coco_context_end();

coco_box_t coco_box_create(int cap);

int coco_box_close(coco_box_t box);

coco_t coco_self();

int coco_run(coco_t *co, coco_routine_t *entry, coco_msg_t arg, coco_t friend);

int coco_send(coco_box_t box, coco_msg_t msg);

int coco_recv(coco_box_t box, coco_msg_t *msg);

int coco_select(coco_way_t ways[], int way_num, int need_block);

void coco_exit();

void coco_yield();

#endif //COCO_COCO_H
