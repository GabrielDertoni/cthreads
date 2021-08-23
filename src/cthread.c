// required for sigaltstack and stack_t
#define _XOPEN_SOURCE 500

#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <cthread.h>

typedef struct {
    jmp_buf context;
    void (*function)(void);
    int active;
    void* stack;
} Task;

/* The fiber "queue" */
static Task task_list[MAX_FIBERS];

/* The index of the currently executing fiber */
static int current_task = -1;
/* A boolean flag indicating if we are in the main process or if we are in a fiber */
static int in_task = 0;
/* The number of active fibers */
static int num_tasks = 0;

/* The "main" execution context */
jmp_buf main_context;

static void usr1_handler_create_stack(int signum) {
    assert(signum == SIGUSR1);

    /* Save the current context, and return to terminate the signal handler scope */
    if (setjmp(task_list[num_tasks].context)) {
        /* We are being called again from the main context. Call the function */
        task_list[current_task].function();
        task_list[current_task].active = 0;
        longjmp(main_context, 1);
    }
    
    return;
}

void init_tasks() {
    for (int i = 0; i < MAX_FIBERS; i++) {
        task_list[i].stack = 0;
        task_list[i].function = 0;
        task_list[i].active = 0;
    }
        
    return;
}

CTHResult spawn_task(void (*func)(void)) {
    
    stack_t stack;
    stack_t old_stack;
    
    if (num_tasks == MAX_FIBERS) return CTH_ERR_MAX_TASKS;
    
    /* Create the new stack */
    stack.ss_flags = 0;
    stack.ss_size = FIBER_STACK;
    stack.ss_sp = malloc(FIBER_STACK);
    if (stack.ss_sp == 0) {
        // LF_DEBUG_OUT( "Error: Could not allocate stack." );
        return CTH_ERR_MALLOC;
    }
    // LF_DEBUG_OUT1( "Stack address from malloc = %p", stack.ss_sp );

    /* Install the new stack for the signal handler */
    if (sigaltstack(&stack, &old_stack)) {
        // LF_DEBUG_OUT("Error: sigaltstack failed.");
        return CTH_ERR_SIGNAL;
    }
    
    /* Install the signal handler */
    /* Sigaction *must* be used so we can specify SA_ONSTACK */
    struct sigaction handler;
    handler.sa_handler = &usr1_handler_create_stack;
    handler.sa_flags = SA_ONSTACK;
    sigemptyset(&handler.sa_mask);

    struct sigaction old_handler;

    if (sigaction(SIGUSR1, &handler, &old_handler)) {
        // LF_DEBUG_OUT( "Error: sigaction failed." );
        return CTH_ERR_SIGNAL;
    }
    
    /* Call the handler on the new stack */
    if (raise(SIGUSR1)) {
        // LF_DEBUG_OUT("Error: raise failed.");
        return CTH_ERR_SIGNAL;
    }
    
    /* Restore the original stack and handler */
    sigaltstack(&old_stack, 0);
    sigaction(SIGUSR1, &old_handler, 0);
    
    /* We now have an additional fiber, ready to roll */
    task_list[num_tasks].active = 1;
    task_list[num_tasks].function = func;
    task_list[num_tasks].stack = stack.ss_sp;

    num_tasks++;
    return CTH_OK;
}

void task_yield() {
    /* If we are in a fiber, switch to the main context */
    if (in_task) {
        /* Store the current state */
        if (setjmp(task_list[current_task].context)) {
            /* Returning via longjmp (resume) */
        } else {
            /* Saved the state: Let's switch back to the main state */
            longjmp(main_context, 1);
        }
    } else {
        /* If we are in main, dispatch the next fiber */
        if (num_tasks == 0) return;
    
        /* Save the current state */
        if (setjmp(main_context)) {
            /* The fiber yielded the context to us */
            in_task = 0;
            if (!task_list[current_task].active) {
                /* If we get here, the fiber returned and is done! */
                
                free(task_list[current_task].stack);
                
                /* Swap the last fiber with the current, now empty, entry */
                num_tasks--;
                if (current_task != num_tasks) {
                    task_list[current_task] = task_list[num_tasks];
                }
                
                /* Clean up the entry */
                task_list[num_tasks].stack = 0;
                task_list[num_tasks].function = 0;
                task_list[num_tasks].active = 0;
            } else {
                // LF_DEBUG_OUT1( "Fiber %d yielded execution.", current_task );
            }
        } else {
            /* Saved the state so call the next fiber */
            current_task = (current_task + 1) % num_tasks;
            
            in_task = 1;
            longjmp(task_list[current_task].context, 1);
        }
    }
    
    return;
}

CTHResult join_all_tasks() {
    int tasks_remaining = 0;
    
    /* If we are in a fiber, wait for all the *other* fibers to quit */
    if (in_task) tasks_remaining = 1;
    
    while (num_tasks > tasks_remaining) {
        task_yield();
    }
    
    return CTH_OK;
}
