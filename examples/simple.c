#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <cthread.h>


void fiber1() {
	for (int i = 0; i < 5; ++i) {
		printf("Hey, I'm fiber #1: %d\n", i);
		task_yield();
	}
	return;
}

void fibonacchi() {
	int fib[2] = { 0, 1 };

	printf("fibonacchi(0) = 0\nfibonnachi(1) = 1\n");
	for (int i = 2; i < 15; ++i) {
		int nextFib = fib[0] + fib[1];
		printf("fibonacchi(%d) = %d\n", i, nextFib);
		fib[0] = fib[1];
		fib[1] = nextFib;
	
        time_t start_time = time(NULL);
        while (time(NULL) - start_time < 2) {
            task_yield();
        }
	}
}

void squares() {
	for (int i = 0; i < 10; ++i) {
		printf("%d*%d = %d\n", i, i, i*i);

        time_t start_time = time(NULL);
        while (time(NULL) - start_time < 1) {
            task_yield();
        }
	}
}

int main()
{
	/* Initialize the fiber library */
	init_tasks();
	
	/* Go fibers! */
	spawn_task(&fiber1);
	spawn_task(&fibonacchi);
	spawn_task(&squares);

	/* Since these are nonpre-emptive, we must allow them to run */
	join_all_tasks();
	
	/* The program quits */
	return 0;
}
