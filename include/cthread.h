#ifndef _CTHREAD_H_
#define _CTHREAD_H_

#define FIBER_STACK 1024 * 64
#define MAX_FIBERS 128

typedef enum {
    CTH_OK,
    CTH_ERR_MAX_TASKS,
    CTH_ERR_MALLOC,
    CTH_ERR_SIGNAL,
} CTHResult;

void init_tasks();
CTHResult spawn_task(void (*func)(void));
void task_yield();
CTHResult join_all_tasks();

#endif
