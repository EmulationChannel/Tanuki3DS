#include "thread.h"

#include "3ds.h"

void load_context(HLE3DS* s) {
    for (int i = 0; i < 16; i++) {
        s->cpu.r[i] = CUR_THREAD.context.r[i];
        s->cpu.d[i] = CUR_THREAD.context.d[i];
    }
    s->cpu.cpsr.w = CUR_THREAD.context.cpsr;
    s->cpu.fpscr.w = CUR_THREAD.context.fpscr;
}

void save_context(HLE3DS* s) {
    for (int i = 0; i < 16; i++) {
        CUR_THREAD.context.r[i] = s->cpu.r[i];
        CUR_THREAD.context.d[i] = s->cpu.d[i];
    }
    CUR_THREAD.context.cpsr = s->cpu.cpsr.w;
    CUR_THREAD.context.fpscr = s->cpu.fpscr.w;
}

void thread_init(HLE3DS* s, u32 entrypoint) {
    s->kernel.cur_tid = thread_create(s, entrypoint, STACK_BASE, 0x30, 0);

    load_context(s);
    CUR_THREAD.state = THRD_RUNNING;
}

u32 thread_create(HLE3DS* s, u32 entrypoint, u32 stacktop, u32 priority,
                  u32 arg) {
    if (SVec_full(s->kernel.threads)) return -1;
    u32 id = SVec_push(s->kernel.threads, ((Thread) {.context.arg = arg,
                                                     .context.sp = stacktop,
                                                     .context.pc = entrypoint,
                                                     .context.cpsr = M_USER,
                                                     .priority = priority,
                                                     .state = THRD_READY}));
    linfo("creating thread %d (entry %08x, stack %08x, priority %x, arg %x)",
          id, entrypoint, stacktop, priority, arg);
    return id;
}

bool thread_reschedule(HLE3DS* s) {
    if (CUR_THREAD.state == THRD_RUNNING) CUR_THREAD.state = THRD_READY;
    u32 nexttid = -1;
    u32 maxprio = 0;
    Vec_foreach(t, s->kernel.threads) {
        if (t->state == THRD_READY && t->priority >= maxprio) {
            maxprio = t->priority;
            nexttid = t - s->kernel.threads.d;
        }
    }
    if (nexttid == -1) {
        s->cpu.wfe = true;
        linfo("all threads sleeping");
        return false;
    }
    linfo("switching thread from %d to %d", s->kernel.cur_tid, nexttid);
    save_context(s);
    s->kernel.cur_tid = nexttid;
    load_context(s);
    return true;
}

u32 syncobj_create(HLE3DS* s, u32 type) {
    if (SVec_full(s->kernel.syncobjs)) return -1;

    u32 id = SVec_push(s->kernel.syncobjs, ((SyncObj) {.type = type}));

    static char* names[SYNCOBJ_MAX] = {[SYNCOBJ_EVENT] = "event",
                                       [SYNCOBJ_MUTEX] = "mutex",
                                       [SYNCOBJ_SEM] = "semaphore"};
    linfo("creating synchronization object %d of type %s", id, names[type]);
    return id;
}

bool syncobj_wait(HLE3DS* s, u32 id) {
    Vec_push(s->kernel.syncobjs.d[id].waiting_thrds, s->kernel.cur_tid);
    CUR_THREAD.state = THRD_SLEEP;
    linfo("waiting on synchronization object %d", id);
    thread_reschedule(s);
    return true;
}

void syncobj_signal(HLE3DS* s, u32 id) {
    linfo("signaling synchronization object %d", id);
    Vec_foreach(t, s->kernel.syncobjs.d[id].waiting_thrds) {
        s->kernel.threads.d[*t].state = THRD_READY;
    }
    Vec_free(s->kernel.syncobjs.d[id].waiting_thrds);
    thread_reschedule(s);
}