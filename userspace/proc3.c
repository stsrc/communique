#include <stdio.h>
#include <unistd.h>
#include "events.h"

int main(void)
{
	char event[2] = "a\0";
	int rt;
	int cnt = 0;
	pid_t pid;
	printf("proc1: event_set\n");
	rt = event_set(event);
	event_check_error_exit(rt, "proc1: event_set");
	printf("proc1: event_set - should ret. error\n");
	rt = event_set(event);
	event_check_error(rt, "proc1: event_set - positive result");
	printf("proc1 pid: %lu\n", (unsigned long)getpid());
	pid = fork();
	switch(pid) {
	case 0:
		printf("proc2: event_set\n");
		rt = event_set(event);
		event_check_error_exit(rt, "proc2 - event_set FAILS!");
		while(cnt < 10) {
			sleep(1);
			printf("proc2: event_wait\n");
			rt = event_wait(event);
			event_check_error_exit(rt, "proc2: event_wait FAILS!");
			sleep(2);
			printf("proc2: event_throw\n");
			rt = event_throw(event);
			event_check_error_exit(rt, "proc2: event_throw FAILS!");
			cnt++;
		}
		printf("proc2: event_unset\n");
		rt = event_unset(event);
		event_check_error_exit(rt, "proc2: event_unset FAILS!");
		exit(0);
	default:
		printf("proc2 pid: %lu\n", (unsigned long)pid);
		pid = fork();
		switch(pid) {
		case 0:
			printf("proc3: event_throw - specially fails!\n");
			rt = event_throw(event);
			event_check_error_exit(rt, "proc3: event_throw - positive result\n");
			printf("proc3: event_throw - negative result!!\n");
			exit(1);
		default:
			printf("proc3 pid: %lu\n", (unsigned long)pid);
			break;
		}
		sleep(2);
		while(cnt < 10) {
			sleep(2);
			printf("proc1: event_throw\n");
			rt = event_throw(event);
			event_check_error_exit(rt, "proc1: event_throw FAILS!");
			sleep(1);
			printf("proc1: event_wait\n");
			rt = event_wait(event);
			event_check_error_exit(rt, "proc1: event_wait FAILS!");
			cnt++;
		}
		printf("proc1: event_unset\n");
		rt = event_unset(event);
		event_check_error_exit(rt, "proc1: event_unset FAILS!");
		exit(0);
	}
	return 0;
}
