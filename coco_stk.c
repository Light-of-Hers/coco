//
// Created by herlight on 2019/9/20.
//

#include "coco_inner.h"
#include <string.h>

// 将coroutine安置到stack上
//1. 如果其绑定的栈不是自己，或是其仍然在栈上，则直接返回
//2. 如果栈上还有别的coroutine，则将其备份
//3. 将目标coroutine的栈拷贝上来
void coco_stk_place_thd(stk_t *me, thd_t *thd) {
    if (thd->bound_stk != me || me->on_stack_thd == thd)
        return;
    if (me->on_stack_thd)
        coco_thd_backup_stack(me->on_stack_thd);
    coco_thd_restore_stack(thd);
    me->on_stack_thd = thd;
}

// 分配一个新的stack
// 因为是需要实际使用的stack，所以需要有guard page
stk_t *coco_new_stk(size_t cap) {
    stk_t *stk = coco_calloc(1, sizeof(*stk));
    link_init(&stk->ctx_ln);
    stack_construct(&stk->shared_stack, cap, TRUE);

    debug("stk[0x%016lx] alloc", (word_t) stk);

    return stk;
}

// 销毁stack
// 只在销毁context时才会使用
void coco_free_stk(stk_t *stk) {
    link_remove(&stk->ctx_ln);
    stack_deconstruct(&stk->shared_stack);
    memset(stk, 0, sizeof(*stk));

    debug("stk[0x%016lx] free", (word_t) stk);

    free(stk);
}