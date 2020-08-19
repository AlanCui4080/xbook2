#ifndef _XBOOK_MSGPOOL_H
#define _XBOOK_MSGPOOL_H

#include <stddef.h>
#include <stdint.h>
#include <xbook/mutexlock.h>
#include <xbook/waitqueue.h>

typedef struct {
    size_t msgsz;       /* message size */
    size_t msgcount;    /* message count */
    size_t msgmaxcnt;   /* message max count */
    uint8_t *head;      /* message head */
    uint8_t *tail;      /* message tail */
    uint8_t *msgbuf;    /* message buf */
    mutexlock_t mutex;  /* message mutex */
    wait_queue_t waiters;   /* message waiters */
} msgpool_t;

msgpool_t *msgpool_create(size_t msgsz, size_t msgcount);
int msgpool_destroy(msgpool_t *pool);
int msgpool_push(msgpool_t *pool, void *buf);
int msgpool_pop(msgpool_t *pool, void *buf);
int msgpool_empty(msgpool_t *pool);
int msgpool_full(msgpool_t *pool);
int msgpool_try_push(msgpool_t *pool, void *buf);
int msgpool_try_pop(msgpool_t *pool, void *buf);

#endif /* _XBOOK_MSGPOOL_H */
