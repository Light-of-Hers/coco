//
// Created by herlight on 2019/9/20.
//

#include "coco.h"
#include "coco_inner.h"

static __thread ctx_t *cur_ctx = 0;

void asm_context_switch(thd_t *thd, void **src_sp, void *dst_sp);
// rdi: thd_t* thd, rsi: void** src_sp, rdx: void* dst_sp
void dummy() {
    __asm__ __volatile__(
    "asm_context_switch:\n"
    "push   %rbp\n"
    "push   %rbx\n"
    "push   %r12\n"
    "push   %r13\n"
    "push   %r14\n"
    "push   %r15\n"

    "mov    %rsp,   (%rsi)\n"
    "mov    %rdx,   %rsp\n"

    "pop    %r15\n"
    "pop    %r14\n"
    "pop    %r13\n"
    "pop    %r12\n"
    "pop    %rbx\n"
    "pop    %rbp\n"
    "ret\n"
    );
}

#define ctx_clear_link(_list, _x) do{ while(!link_empty(&_list)) \
    coco_free_##_x(master(_list.next, _x##_t, ctx_ln)); } while(0)
;
// 调度器例程，一个无限循环
//1. 如果当前的coroutine退出，则进行销毁工作
//2. 运行ready队列头的coroutine
//  a. 状态设置
//  b. 安置到栈上
//  c. 上下文切换
static void scheduler() {
    while (1) {
        if (cur_ctx->active_thd->state == COCO_EXITED)
            coco_free_thd(cur_ctx->active_thd);

        panic_if(link_empty(&cur_ctx->ready_thd_queue), "no runnable routine");
        link_t *ln = link_dequeue(&cur_ctx->ready_thd_queue);
        thd_t *new_thd = master(ln, thd_t, ctx_ln);

        new_thd->state = COCO_RUNNING;
        cur_ctx->active_thd = new_thd;

        coco_stk_place_thd(new_thd->bound_stk, new_thd);

        asm_context_switch(new_thd, &cur_ctx->scheduler_sp, new_thd->stack_info.sp);
    }
}

#define SCHEDULER_STACK_CAP (16 * KB)

// 进入调度器例程
static inline void coco_schedule() {
    asm_context_switch(0, &cur_ctx->active_thd->stack_info.sp, cur_ctx->scheduler_sp);
}

// 当前coroutine睡眠
//1. 状态设置
//2. 加入睡眠列表
//3. 调度
static inline void coco_sleep() {
    thd_t *thd = cur_ctx->active_thd;
    thd->state = COCO_SLEEP;

    link_push_back(&thd->owner_ctx->sleep_thd_list, &thd->ctx_ln);

    coco_schedule();
}

//　coroutine的公共入口
void coco_entry(thd_t *cur_thd) {
    cur_thd->entry(cur_thd->init_arg);
    coco_exit();
}

//　初始化coroutine上下文，一个线程只能初始化一个
//1. 初始化各个列表和队列
//2. 初始化调度器使用的栈
//3. 初始化主coroutine
void coco_context_start() {
    panic_if(cur_ctx, "current thread's context already started");

    cur_ctx = coco_calloc(1, sizeof(*cur_ctx));

    link_init(&cur_ctx->ready_thd_queue);
    link_init(&cur_ctx->sleep_thd_list);
    link_init(&cur_ctx->box_list);
    link_init(&cur_ctx->free_stk_list);

    stack_construct(&cur_ctx->scheduler_stack, SCHEDULER_STACK_CAP, TRUE);
    cur_ctx->scheduler_sp = stack_init(&cur_ctx->scheduler_stack, (void *) &scheduler);

    cur_ctx->main_thd.magic = THD_MAGIC;
    cur_ctx->main_thd.owner_ctx = cur_ctx;
    cur_ctx->main_thd.state = COCO_RUNNING;
    coco_thd_bind_stk(&cur_ctx->main_thd, &cur_ctx->fake_stk);

    cur_ctx->active_thd = &cur_ctx->main_thd;
}

//　结束coroutine上下文，必须在主coroutine中结束
//1. 销毁各个列表和队列
//2. 销毁调度器栈
//3. 销毁context数据结构，将cur_ctx置零
void coco_context_end() {
    panic_if(!cur_ctx, "current thread's context hasn't started yet");
    panic_if(cur_ctx->active_thd != &cur_ctx->main_thd, "only main routine can end the context");

    ctx_clear_link(cur_ctx->sleep_thd_list, thd);
    ctx_clear_link(cur_ctx->ready_thd_queue, thd);
    ctx_clear_link(cur_ctx->box_list, box);
    ctx_clear_link(cur_ctx->free_stk_list, stk);

    stack_deconstruct(&cur_ctx->scheduler_stack);

    memset(cur_ctx, 0, sizeof(*cur_ctx));

    free(cur_ctx), cur_ctx = 0;
}

// 获得当前coroutine ID
coco_t coco_self() {
    return (coco_t) cur_ctx->active_thd;
}

// 退出当前coroutine
void coco_exit() {
    thd_t *thd = cur_ctx->active_thd;
    thd->state = COCO_EXITED;
    coco_schedule();
    panic("you cannot touch here");
}

// 出让CPU
void coco_yield() {
    thd_t *thd = cur_ctx->active_thd;
    thd->state = COCO_READY;
    link_enqueue(&cur_ctx->ready_thd_queue, &thd->ctx_ln);
    coco_schedule();
}

static inline bool_t box_valid(box_t *box) {
    return box->magic == BOX_MAGIC;
}

static inline bool_t thd_valid(thd_t *thd) {
    return thd->magic == THD_MAGIC;
}

// 创建一个新box
coco_box_t coco_box_create(int cap) {
    return (coco_box_t) coco_new_box(cur_ctx, cap);
}

//　关闭box
int coco_box_close(coco_box_t box_) {
    box_t *box = (box_t *) box_;
    if (box->magic != BOX_MAGIC)
        return -1;
    coco_free_box(box);
    return 0;
}

#define STACK_CAP (128 * KB)

// 启动一个新coroutine（暂时没有运行）
int coco_run(coco_t *co, coco_routine_t *entry, coco_msg_t arg, coco_t friend) {
    thd_t *thd;

    thd_t *frd = (thd_t *) friend;

    if (frd && thd_valid(frd)) {
        panic_if(frd->owner_ctx != cur_ctx, "a friend from other context");
        thd = coco_new_thd(cur_ctx, frd->bound_stk);
    } else if (!link_empty(&cur_ctx->free_stk_list)) {
        link_t *ln = link_dequeue(&cur_ctx->free_stk_list);
        thd = coco_new_thd(cur_ctx, master(ln, stk_t, ctx_ln));
    } else {
        thd = coco_new_thd(cur_ctx, coco_new_stk(STACK_CAP));
    }

    thd->entry = entry;
    thd->init_arg = arg;

    if (co)
        *co = (coco_t) thd;

    return 0;
}

static inline void alloc_alarm(thd_t *thd, int n) {
    thd->alm_num = n;
    thd->alms = coco_malloc(n * sizeof(alarm_t));
}

static inline void free_alarm(thd_t *thd) {
    thd->alm_num = 0;
    free(thd->alms), thd->alms = 0;
}

// 向box发送信息
//1. 检查box的合法性
//2. 尝试发信
//3. 发信失败，则睡眠
int coco_send(coco_box_t box_, coco_msg_t msg) {
    box_t *box = (box_t *) box_;
    if (!box_valid(box))
        return -1;
    panic_if(box->owner_ctx != cur_ctx, "a box from other context");
    if (coco_box_try_send(box, msg))
        return 0;

    thd_t *thd = cur_ctx->active_thd;

    alloc_alarm(thd, 1);
    alarm_t *alm = thd->alms;
    alarm_construct(alm, thd, box, msg, TRUE);

    box->is_send = TRUE;
    coco_sleep();

    if (!box_valid(box) || box->id != alm->box_id) {
        free_alarm(thd);
        return -1;
    }

    free_alarm(thd);
    return 0;
}

//　从box接收信息
int coco_recv(coco_box_t box_, coco_msg_t *msg) {
    box_t *box = (box_t *) box_;
    if (!box_valid(box))
        return -1;
    panic_if(box->owner_ctx != cur_ctx, "a box from other context");
    if (coco_box_try_recv(box, msg))
        return 0;

    thd_t *thd = cur_ctx->active_thd;

    alloc_alarm(thd, 1);
    alarm_t *alm = thd->alms;
    alarm_construct(alm, thd, box, 0, FALSE);

    box->is_send = FALSE;
    coco_sleep();

    if (!box_valid(box) || box->id != alm->box_id) {
        free_alarm(thd);
        return -1;
    }

    if (msg)
        *msg = alm->msg;
    free_alarm(thd);
    return 0;
}
//　多路收发，返回成功收发信息的路（可能是已经关闭的路）的idx
//　如果不需要阻塞，且每一路都无法即时收发，则返回-1
int coco_select(coco_way_t ways[], int way_num, int need_block) {
    thd_t *thd = cur_ctx->active_thd;
    for (int i = 0; i < way_num; ++i) {
        box_t *box = (box_t *) ways[i].box;
        if (!box_valid(box)) {
            ways[i].valid = 0;
            return i;
        } else {
            panic_if(box->owner_ctx != cur_ctx, "a box from other context");
            ways[i].valid = 1;
            if (((ways[i]).is_send && coco_box_try_send(box, ways[i].msg)) ||
                coco_box_try_recv(box, &ways[i].msg))
                return i;
        }
    }
    if (!need_block)
        return -1;

    alloc_alarm(thd, way_num);
    for (int i = 0; i < way_num; ++i) {
        if (!ways[i].valid)
            continue;
        box_t *box = (box_t *) ways[i].box;
        alarm_t *alm = thd->alms + i;
        alarm_construct(alm, thd, box, ways[i].msg, ways[i].is_send);
    }
    coco_sleep();

    int idx = (int) (thd->ring - thd->alms);
    panic_if(idx >= thd->alm_num, "runtime error");

    ways[idx].msg = thd->ring->msg;
    box_t *box = (box_t *) ways[idx].box;
    if (!box_valid(box) || thd->ring->box_id != box->id)
        ways[idx].valid = 0;

    free_alarm(thd);
    return idx;
}
