#include <stdio.h>
#include <unistd.h>
#include "events.h"

void proc2(char **events)
{
	int rt;
	printf("proc2 - event_set\n");
	rt = event_set(events[1]);
	event_check_error_exit(rt, "proc2 - event_set, FAILED - SHOULD NOT FAIL!");
	printf("proc2 - event_wait\n");
	rt = event_wait(events[0]);
	event_check_error_exit(rt, "proc2 - event_wait, FAILED - SHOULD NOT FAIL!");
	printf("proc2 - event_throw\n");
	rt = event_throw(events[1]);
	event_check_error_exit(rt, "proc2 - event_throw, FAILED - SHOULD NOT FAIL!");
	printf("proc2 - event_unset\n");
	rt = event_unset(events[1]);
	event_check_error_exit(rt, "proc2 - event_unset, FAILED - SHOULD NOT FAIL!");
	printf("proc2 - dead\n");
	exit(0);	
}

void proc3(char **events)
{
	int rt;
	printf("proc3 - event_set\n");
	rt = event_set(events[2]);
	event_check_error_exit(rt, "proc3 - event_set, FAILED - SHOULD NOT FAIL!");
	printf("proc3 - event_wait\n");
	rt = event_wait(events[1]);
	event_check_error_exit(rt, "proc3 - event_wait, FAILED - SHOULD NOT FAIL!");
	printf("proc3 - event_unset\n");
	rt = event_unset(events[2]);
	event_check_error_exit(rt, "proc3 - event_unset, FAILED - SHOULD NOT FAIL!");
	printf("proc3 - dead\n");
	exit(0);	
}

void proc4(char **events)
{
	int rt;
	printf("proc4 - event_set\n");
	rt = event_set(events[2]);
	event_check_error_exit(rt, "proc4 - event_set, FAILED - SHOULD NOT FAIL!");
	sleep(3);
	printf("proc4 - event_throw\n");
	rt = event_throw(events[2]);
	event_check_error_exit(rt, "proc4 - event_throw, FAILED - SHOULD NOT FAIL!");
	printf("proc4 - event_unset\n");
	rt = event_unset(events[2]);
	event_check_error_exit(rt, "proc4 - event_unset, FAILED - SHOULD NOT FAIL!");
	printf("proc4 - dead\n");
	exit(0);
}

int main(void)
{
	char *events[] = {"a\0", "b\0", "c\0"};
	pid_t pid;
	printf("proc1 - event_set\n");
	int rt = event_set(events[0]);
	event_check_error_exit(rt, "proc1 - event_set, FAILED - SHOULD NOT FAIL!");
	pid = fork();
	switch(pid) {
	case 0:
		proc2(events);
		break;
	default:
		break;
	}
	sleep(1);
	pid = fork();
	switch (pid) {
	case 0:
		proc3(events);
		break;
	default:
		break;
	}
	sleep(1);
	pid = fork();
	switch (pid) {
	case 0:
		proc4(events);
		break;
	default:
		break;
	}
	sleep(1);
	printf("proc1 - event_wait\n");
	rt = event_wait(events[2]);
	event_check_error_exit(rt, "proc1 - event_wait, FAILED - SHOULD NOT FAIL!");
	printf("proc1 - event_throw\n");
	rt = event_throw(events[0]);
	event_check_error_exit(rt, "proc1 - event_throw, FAILED - SHOULD NOT FAIL!");
	sleep(5);
	printf("proc1 - event_unset\n");
	rt = event_unset(events[0]);
	event_check_error_exit(rt, "proc1 - event_unset, FAILED - SHOULD NOT FAIL!");	
	printf("proc1 - dead\n");
	return 0;
}
