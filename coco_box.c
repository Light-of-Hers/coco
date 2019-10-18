//
// Created by herlight on 2019/9/20.
//

#include "coco_inner.h"
#include <string.h>

// 尝试发信
//+ 如果闹钟队列是空的，则尝试往缓冲区发信
//+ 如果闹钟队列非空，且处于收信阻塞状态，则鸣响队首的闹钟，否则一起阻塞
bool_t coco_box_try_send(box_t *me, coco_msg_t msg) {
    if (link_empty(&me->alarm_queue)) {
        if (me->buf.cnt == me->buf.cap) {
            return FALSE;
        } else {
            me->buf.msgs[(me->buf.head + me->buf.cnt) % me->buf.cap] = msg;
            me->buf.cnt++;
        }
    } else {
        if (me->is_send) {
            return FALSE;
        } else {
            // 收信阻塞，说明缓冲区为空

            alarm_t *alm = master(link_queue_head(&me->alarm_queue), alarm_t, box_ln);
            alm->trans_done = TRUE;
            alm->msg = msg;

            thd_t *thd = alm->owner_thd;
            thd->ring = alm;
            coco_thd_wakeup(thd);
        }
    }
    return TRUE;
}

//　尝试收信
bool_t coco_box_try_recv(box_t *me, coco_msg_t *msg) {
    if (link_empty(&me->alarm_queue)) {
        if (me->buf.cnt == 0) {
            return FALSE;
        } else {
            if (msg)
                *msg = me->buf.msgs[me->buf.head];
            me->buf.head = (me->buf.head + 1) % me->buf.cap;
            me->buf.cnt--;
        }
    } else {
        if (!me->is_send) {
            return FALSE;
        } else {
            // 发信阻塞，说明缓冲区已满

            alarm_t *alm = master(link_queue_head(&me->alarm_queue), alarm_t, box_ln);
            alm->trans_done = TRUE;
            if (me->buf.cap > 0) {
                if (msg)
                    *msg = me->buf.msgs[me->buf.head]; // 收到缓冲区头部的信息
                me->buf.msgs[me->buf.head] = alm->msg; // 把闹钟携带的信息加到缓冲区尾部
                me->buf.head = (me->buf.head + 1) % me->buf.cap;
            } else {
                if (msg)
                    *msg = alm->msg;
            }

            thd_t *thd = alm->owner_thd;
            thd->ring = alm;
            coco_thd_wakeup(thd);
        }
    }
    return TRUE;
}

// 分配一个新的box
//1. 魔数初始化
//2. 闹钟队列和消息缓冲区初始化
//3. 绑定context
box_t *coco_new_box(ctx_t *ctx, int cap) {
    box_t *box = coco_calloc(1, sizeof(*box));

    box->magic = BOX_MAGIC;

    link_init(&box->alarm_queue);
    box->buf.cap = cap;
    box->buf.msgs = coco_malloc(cap * sizeof(coco_msg_t));

    box->owner_ctx = ctx;
    box->id = ctx->cur_box_id++;
    link_push_back(&ctx->box_list, &box->ctx_ln);

    debug("box[0x%016lx] alloc", (word_t) box);

    return box;
}

// 销毁box
//1. 从context的列表中移除
//2. 唤醒所有沉睡在其上的coroutine
//3. 销毁缓冲区
//4. 内存清零
void coco_free_box(box_t *box) {
    link_remove(&box->ctx_ln);

    while (!link_empty(&box->alarm_queue)) {
        link_t *ln = link_dequeue(&box->alarm_queue);
        alarm_t *alm = master(ln, alarm_t, box_ln);
        thd_t *thd = alm->owner_thd;
        thd->ring = alm;
        coco_thd_wakeup(thd);
    }

    if (box->buf.cap > 0)
        free(box->buf.msgs), box->buf.msgs = 0;

    memset(box, 0, sizeof(*box));

    debug("box[0x%016lx] free", (word_t) box);

    free(box);
}

