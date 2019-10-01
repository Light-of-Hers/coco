//
// Created by herlight on 2019/9/20.
//

#include "coco_inner.h"
#include <string.h>

#define INIT_STACK_CAP (sizeof(word_t) * (CALLEE_SAVED_REG_NUM + 1))

// 备份coroutine的栈
void coco_thd_backup_stack(thd_t *me) {
    size_t cap = me->stack_info.bot - me->stack_info.sp;
    stack_construct(&me->backup_stack, cap, FALSE);
    memmove(me->backup_stack.mem, me->stack_info.sp, cap);
}

// 恢复coroutine的栈
void coco_thd_restore_stack(thd_t *me) {
    size_t cap = me->backup_stack.cap;
    memmove(me->stack_info.sp, me->backup_stack.mem, cap);
    stack_deconstruct(&me->backup_stack);
}

// 移除coroutine的所有alarm
static inline void remove_alarms(thd_t *me) {
    if (me->alm_num > 0) {
        for (int i = 0; i < me->alm_num; ++i) {
            link_remove(&me->alms[i].box_ln);
        }
    }
}

// 唤醒coroutine
//1. 改变状态为就绪态
//2. 从sleep队列迁移到ready队列
//3. 移除所有的闹钟
void coco_thd_wakeup(thd_t *me) {
    assert(me->owner_ctx->active_thd != me);

    me->state = COCO_READY;

    link_remove(&me->ctx_ln);
    link_enqueue(&me->owner_ctx->ready_thd_queue, &me->ctx_ln);

    remove_alarms(me);
}

// 将coroutine与一个co-stack绑定
// 在coroutine初始化时进行
//1. 设置指向co-stack的引用
//2. 设置coroutine的栈底和栈顶
//3. co-stack的引用计数加一
void coco_thd_bind_stk(thd_t *me, stk_t *stk) {
    me->bound_stk = stk;
    me->stack_info.bot = stk->shared_stack.mem + stk->shared_stack.cap;
    me->stack_info.sp = me->stack_info.bot - INIT_STACK_CAP;
    stk->ref_cnt++;
}

// 分配一个新的coroutine
//1. 魔数
//2. 构建初始的备份栈
//3. 与context绑定
//4. 与stack绑定
thd_t *coco_new_thd(ctx_t *ctx, stk_t *stk) {
    thd_t *thd = coco_calloc(1, sizeof(*thd));

    thd->magic = THD_MAGIC;

    stack_construct(&thd->backup_stack, INIT_STACK_CAP, FALSE);
    stack_init(&thd->backup_stack, (void *) &coco_entry);

    thd->owner_ctx = ctx;
    thd->state = COCO_READY;
    link_enqueue(&ctx->ready_thd_queue, &thd->ctx_ln);

    coco_thd_bind_stk(thd, stk);

    return thd;
}

// 销毁一个coroutine
//1. 从context的队列移除
//2. 移除所有的alarm
//3. 析构备份栈
//4. 减少stack的引用，如果减到0则加入context的空间stack队列
//5. 内存清零，防止之后再被引用
void coco_free_thd(thd_t *thd) {

    link_remove(&thd->ctx_ln);

    remove_alarms(thd);
    if (thd->alm_num > 0)
        free(thd->alms), thd->alms = 0;

    stack_deconstruct(&thd->backup_stack);

    if (--thd->bound_stk->ref_cnt == 0)
        link_push_back(&thd->owner_ctx->free_stk_list, &thd->bound_stk->ctx_ln);

    memset(thd, 0, sizeof(*thd));

    free(thd), thd = 0;
}
