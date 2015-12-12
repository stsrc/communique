#include <stdio.h>
#include <unistd.h>
#include "events.h"
/*
 * test scenario:
 * proc2 throws b, waits first for a
 * proc3 throws c, waits second for b
 * proc1 throws a, tries to wait for c
 */

void print_test_scenario()
{
	printf("\ntest scenario:\n\nproc2 throws \"b\", waits as a first"
		" for event \"a\"\n"
		"proc3 throws \"c\", waits as a second for \"b\"\n"
		"proc1 throws \"a\", tries to wait for a \"c\""
	        " - and it fails\n\n");
}
void proc2(char **events)
{
	int e0, e1, rt;
	printf("proc2 - event_set(events[0])\n");
	e0 = event_set(events[0]);
	printf("proc2 - event_set(events[1])\n");
	e1 = event_set(events[1]);
	event_check_error_exit(e0, "proc2 - event_set");
	event_check_error_exit(e1, "proc2 - event_set");
	printf("proc2 - event_wait(e0)\n");
	rt = event_wait(e0);
	event_check_error_exit(rt, "proc2 - event_wait");
	printf("proc2 - event_throw(e1)\n");
	rt = event_throw(e1);
	event_check_error_exit(rt, "proc2 - event_throw");
	printf("proc2 - event_unset(e1)\n");
	e1 = event_unset(e1);
	event_check_error_exit(e1, "proc2 - event_unset");
	printf("proc2 - event_unset(e0)\n");
	e0 = event_unset(e0);
	event_check_error_exit(e0, "proc2 - event_unset");
	printf("proc2 exits\n");
	exit(0);	
}

void proc3(char **events)
{
	int rt, e1, e2;
	printf("proc3 - event_set(events[2])\n");
	e2 = event_set(events[2]);
	printf("proc3 - event_set(events[1])\n");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc3 - event_set");
	event_check_error_exit(e2, "proc3 - event_set");
	printf("proc3 - event_wait(e1)\n");
	rt = event_wait(e1);
	event_check_error_exit(rt, "proc3 - event_wait");
	printf("proc3 - event_unset(e2)\n");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc3 - event_unset");
	printf("proc3 - event_unset(e1)\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc3 - event_unset");
	printf("proc3 exits\n");
	exit(0);	
}

int main(void)
{
	char *events[] = {"a\0", "b\0", "c\0"};
	pid_t pid;
	int e0, e1, e2, rt;
	print_test_scenario();
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc2(events);
		break;
	default:
		break;
	}
	pid = fork();
	switch (pid) {
	case 0:
		sleep(2);
		proc3(events);
		break;
	default:
		break;
	}
	printf("proc1 - event_set(events[0])\n");
	e0 = event_set(events[0]);
	event_check_error_exit(e0, "proc1 - event_set");
	printf("proc1 - event_set(events[1])\n");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc1 - event_set");
	printf("proc1 - event_set(events[2])\n");
	e2 = event_set(events[2]);
	sleep(5);
	printf("proc1 - event_wait(e2) - deadlock error should occure\n");
	rt = event_wait(e2);
	event_check_error(rt, "proc1 - event_wait. Positive fail");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc1 - event_throw");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1 - event_unset");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc1 - event_unset");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc1 - event_unset");
	sleep(1);
	printf("proc1 exits\n");
	return 0;
}
