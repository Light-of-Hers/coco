//
// Created by herlight on 2019/9/20.
//

#ifndef COCO_COCO_INNER_H
#define COCO_COCO_INNER_H

#include "coco_util.h"
#include "coco.h"
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>

#define CALLEE_SAVED_REG_NUM 6
#define THD_MAGIC 0xdeadbeafdeadface
#define BOX_MAGIC 0xdeafbeaffacebeaf

typedef struct ctx_s ctx_t;
typedef struct thd_s thd_t;
typedef struct stk_s stk_t;
typedef struct box_s box_t;

typedef struct {
    size_t cap;
    void *mem;
    bool_t has_guard_page;
} stack_t;

// 构造stack
//+ 如果需要guard page，则采用mmap等系统调用映射内存，并改写guard page的访问权限
//+ 否则，直接用malloc申请内存
static inline void stack_construct(stack_t *me, size_t cap, bool_t has_guard_page) {
    me->has_guard_page = has_guard_page;
    if (!has_guard_page) {
        me->mem = coco_realloc(me->mem, cap), me->cap = cap;
    } else {
        size_t page_size = getpagesize();
        cap = round_up(cap, page_size) + page_size;
        me->cap = cap;
        me->mem = mmap(NULL, cap, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        panic_if(me->mem == MAP_FAILED, "mmap failed");
        panic_if(mprotect(me->mem, page_size, PROT_READ) < 0, "mprotect failed");
    }
}

// 析构stack
static inline void stack_deconstruct(stack_t *me) {
    if (!me->has_guard_page) {
        if (me->mem)
            free(me->mem), me->cap = 0, me->mem = 0;
    } else {
        panic_if(munmap(me->mem, me->cap) < 0, "munmap failed"), me->cap = 0, me->mem = 0;
    }
}

// 初始化一个coroutine的栈，返回栈顶指针
// 名字可能取得不是很好？
//1. 返回地址设为entry
//2. callee-saved寄存器初始化为0
static inline void *stack_init(stack_t *me, void *entry) {
    word_t *sp = me->mem + me->cap;
    *(--sp) = (word_t) entry;
    sp -= CALLEE_SAVED_REG_NUM;
    memset(sp, 0, CALLEE_SAVED_REG_NUM * sizeof(word_t));
    return sp;
}

struct box_s {
    uint64_t magic;
    int id; // 用于通信时校验是否是已经关闭的邮箱

    link_t ctx_ln;

    ctx_t *owner_ctx;

    bool_t is_send;

    link_t alarm_queue;

    struct {
        coco_msg_t *msgs;
        int cap, head, cnt;
    } buf;
};

typedef struct {
    thd_t *owner_thd;
    coco_msg_t msg;
    link_t box_ln;
    int box_id;
    bool_t is_send;
} alarm_t;

// 构造一个alarm，并将其推入box的队列
static inline void
alarm_construct(alarm_t *me, thd_t *thd, box_t *box, coco_msg_t msg, bool_t is_send) {
    me->owner_thd = thd;
    me->msg = msg;
    me->is_send = is_send;
    me->box_id = box->id;
    link_enqueue(&box->alarm_queue, &me->box_ln);
}

typedef enum {
    COCO_READY,
    COCO_SLEEP,
    COCO_RUNNING,
    COCO_EXITED,
} state_t;

struct thd_s {
    uint64_t magic;

    link_t ctx_ln;

    ctx_t *owner_ctx;
    stk_t *bound_stk;

    struct {
        void *bot;
        void *sp;
    } stack_info;

    state_t state;

    stack_t backup_stack;

    alarm_t *alms;
    int alm_num;
    alarm_t *ring;

    coco_routine_t *entry;

    coco_msg_t init_arg;
};

struct stk_s {
    link_t ctx_ln;

    int ref_cnt;

    thd_t *on_stack_thd;

    stack_t shared_stack;
};

struct ctx_s {
    link_t free_stk_list;
    link_t box_list;
    link_t ready_thd_queue;
    link_t sleep_thd_list;

    stack_t scheduler_stack;
    void *scheduler_sp;

    thd_t *active_thd;

    stk_t fake_stk;
    thd_t main_thd;

    int cur_box_id;
};

thd_t *coco_new_thd(ctx_t *ctx, stk_t *stk);

void coco_free_thd(thd_t *thd);

stk_t *coco_new_stk(size_t cap);

void coco_free_stk(stk_t *stk);

box_t *coco_new_box(ctx_t *ctx, int cap);

void coco_free_box(box_t *box);

void coco_thd_backup_stack(thd_t *me);

void coco_thd_restore_stack(thd_t *me);

void coco_thd_wakeup(thd_t *me);

void coco_thd_bind_stk(thd_t *me, stk_t *stk);


void coco_stk_place_thd(stk_t *me, thd_t *thd);


bool_t coco_box_try_send(box_t *me, coco_msg_t msg);

bool_t coco_box_try_recv(box_t *me, coco_msg_t *msg);


void coco_entry(thd_t *cur_thd);


#endif //COCO_COCO_INNER_H