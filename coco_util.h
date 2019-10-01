//
// Created by herlight on 2019/9/20.
//

#ifndef COCO_COCO_UTIL_H
#define COCO_COCO_UTIL_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

typedef uintptr_t word_t;
typedef enum {
    FALSE = 0,
    TRUE = 1,
} bool_t;

#define KB (1U << 10)
#define MB (1U << 20)

#define STR_IMPL(_x) #_x
#define STR(_x) STR_IMPL(_x)

#define log(_args...) (fprintf(stderr, __FILE__ ":" STR(__LINE__) ":" _args), fputc('\n', stderr))
#define panic(_args...) (log("panic: "_args), exit(1))
#define warn(_args...) log("warning: "_args)
#define do_if(_cond, _action, _args...) ((_cond) ? (_action(_args)) : (void)0)
#define panic_if(_cond, _args...) do_if(_cond, panic, _args)
#define warn_if(_cond, _args...) do_if(_cond, warn, _args)

static inline word_t round_down(word_t x, word_t f) {
    return x / f * f;
}

static inline word_t round_up(word_t x, word_t f) {
    return (x + f - 1) / f * f;
}

typedef struct link_s link_t;
struct link_s {
    link_t *prev, *next;
};

static inline void link_init(link_t *ln) {
    ln->prev = ln->next = ln;
}

static inline int link_empty(link_t *ln) {
    if (ln->next == ln) {
        assert(ln->prev == ln);
        return 1;
    } else {
        return 0;
    }
}

static inline void link_push_front(link_t *cur, link_t *ln) {
    ln->prev = cur, ln->next = cur->next;
    ln->prev->next = ln->next->prev = ln;
}

static inline void link_push_back(link_t *cur, link_t *ln) {
    link_push_front(cur->prev, ln);
}

static inline link_t *link_remove(link_t *ln) {
    ln->prev->next = ln->next, ln->next->prev = ln->prev;
    link_init(ln);
    return ln;
}

static inline link_t *link_pop_front(link_t *cur) {
    return link_remove(cur->next);
}

static inline link_t *link_pop_back(link_t *cur) {
    return link_remove(cur->prev);
}

static inline void link_enqueue(link_t *queue, link_t *node) {
    link_push_back(queue, node);
}

static inline link_t *link_dequeue(link_t *queue) {
    return link_pop_front(queue);
}

static inline link_t *link_queue_head(link_t *queue) {
    return queue->next;
}

static inline void *coco_malloc(size_t sz) {
    void *ptr = malloc(sz);
    panic_if(!ptr, "out of memory");
    return ptr;
}

static inline void *coco_calloc(size_t n, size_t sz) {
    void *ptr = calloc(n, sz);
    panic_if(!ptr, "out of memory");
    return ptr;
}

static inline void *coco_realloc(void *ptr, size_t sz) {
    ptr = realloc(ptr, sz);
    panic_if(!ptr, "out of memory");
    return ptr;
}

#define offset(_t, _m) ((word_t)(&((_t*)0)->_m))
#define master(_x, _t, _m) ((_t*)((void*)(_x) - offset(_t, _m)))


#endif //COCO_COCO_UTIL_H
