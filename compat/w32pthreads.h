/*
 * w32pthreads compat layer - XP compatible version
 * No SRW Locks, no Vista+ APIs, with lazy CS initialization
 * pthread_once emulated via InterlockedCompareExchange
 */

#ifndef COMPAT_W32PTHREADS_H
#define COMPAT_W32PTHREADS_H

#include <windows.h>
#include <errno.h>
#include <process.h>
#include <time.h>

typedef struct {
    CRITICAL_SECTION cs;
} pthread_mutex_t;

#define PTHREAD_MUTEX_INITIALIZER { { 0 } }

typedef struct {
    HANDLE event;
} pthread_cond_t;

#define PTHREAD_COND_INITIALIZER { NULL }

typedef struct {
    HANDLE thread;
    void *(*start_routine)(void *);
    void *arg;
} pthread_t;

/* ---- Mutex ---- */

static inline int pthread_mutex_init(pthread_mutex_t *m, void *attr)
{
    (void)attr;
    InitializeCriticalSection(&m->cs);
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *m)
{
    if (!m->cs.DebugInfo)
        InitializeCriticalSection(&m->cs);
    EnterCriticalSection(&m->cs);
    return 0;
}

static inline int pthread_mutex_trylock(pthread_mutex_t *m)
{
    if (!m->cs.DebugInfo)
        InitializeCriticalSection(&m->cs);
    return TryEnterCriticalSection(&m->cs) ? 0 : EBUSY;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *m)
{
    LeaveCriticalSection(&m->cs);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *m)
{
    DeleteCriticalSection(&m->cs);
    return 0;
}

/* ---- CondVar (emulated via Event) ---- */

static inline int pthread_cond_init(pthread_cond_t *c, void *attr)
{
    (void)attr;
    c->event = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!c->event) return ENOMEM;
    return 0;
}

static inline int pthread_cond_signal(pthread_cond_t *c)
{
    if (!c->event) return EINVAL;
    SetEvent(c->event);
    return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t *c)
{
    if (!c->event) return EINVAL;
    SetEvent(c->event);
    return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m)
{
    if (!c->event) return EINVAL;
    pthread_mutex_unlock(m);
    WaitForSingleObject(c->event, INFINITE);
    pthread_mutex_lock(m);
    return 0;
}

static inline int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m,
                                          const void *abstime)
{
    DWORD ms = INFINITE;
    (void)abstime;
    if (!c->event) return EINVAL;
    pthread_mutex_unlock(m);
    WaitForSingleObject(c->event, ms);
    pthread_mutex_lock(m);
    return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t *c)
{
    if (c->event) {
        CloseHandle(c->event);
        c->event = NULL;
    }
    return 0;
}

/* ---- Thread ---- */

static unsigned __stdcall pthread_start_wrapper(void *arg)
{
    pthread_t *t = (pthread_t *)arg;
    t->start_routine(t->arg);
    return 0;
}

static inline int pthread_create(pthread_t *t, void *attr,
                                 void *(*start_routine)(void *), void *arg)
{
    (void)attr;
    t->start_routine = start_routine;
    t->arg = arg;
    t->thread = (HANDLE)_beginthreadex(NULL, 0, pthread_start_wrapper, t, 0, NULL);
    if (!t->thread) return EAGAIN;
    return 0;
}

static inline int pthread_join(pthread_t t, void **retval)
{
    (void)retval;
    WaitForSingleObject(t.thread, INFINITE);
    CloseHandle(t.thread);
    return 0;
}

static inline int pthread_detach(pthread_t t)
{
    CloseHandle(t.thread);
    return 0;
}

static inline void pthread_exit(void *retval)
{
    (void)retval;
    _endthreadex(0);
}

static inline pthread_t pthread_self(void)
{
    pthread_t t;
    t.thread = GetCurrentThread();
    t.start_routine = NULL;
    t.arg = NULL;
    return t;
}

/* ---- pthread_once (XP-compatible, no INIT_ONCE) ---- */

typedef volatile LONG pthread_once_t;
#define PTHREAD_ONCE_INIT 0

static inline int pthread_once(pthread_once_t *once, void (*func)(void))
{
    if (*once == 0) {
        /* Try to be the one to run init.
         * InterlockedCompareExchange returns the previous value.
         * If previous was 0, we won the race.
         */
        if (InterlockedCompareExchange(once, 1, 0) == 0) {
            func();
            InterlockedExchange(once, 2); /* mark as done */
        } else {
            /* Another thread is running init, spin-wait */
            while (*once == 1) {
                Sleep(1);
            }
        }
    }
    return 0;
}

/* ---- Misc ---- */

#define pthread_equal(a, b) ((a).thread == (b).thread)

#endif /* COMPAT_W32PTHREADS_H */
