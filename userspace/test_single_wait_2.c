#include <stdio.h>
#include <unistd.h>
#include "events.h"

/*
 * TEST SCENARIO:
 *3 processes - proc1 throws, proc2 and proc3 waits.
 */

void print_test_scenario()
{	
	printf("\nTest scenario:\n\n");
	printf("proc1 throws event\n");
	printf("proc2 and proc3 waits for event\n\n");
}
void proc1(char *event)
{
	int e0;
	printf("proc1 - event_set(a)\n");
	e0 = event_set(event);
	event_check_error(e0, "main: event_set");
	int cnt = 0;
	int rt = 0;
	sleep(2);
	while(cnt < 4) {
		sleep(2);
		printf("proc1 - event_throw(a)\n");
		rt = event_throw(e0);
		event_check_error(rt, "proc1 - event_throw");
		cnt++;
	}
	printf("proc1 - event_unset(a)\n");
	rt = event_unset(e0);
	event_check_error(rt, "proc1 - event_unset");
	while(rt < 0) {
		event_throw(e0);
		rt = event_unset(e0);
		event_check_error(rt, "proc1 - event_unset");
	}
	printf("proc1 - exits.\n");
	sleep(1);
	exit(0);
}

void proc2(char *event)
{
	int rt;
	int cnt = 0;
	printf("proc2 - event_set(a)\n");
	int e0 = event_set(event);
	event_check_error_exit(e0, "proc2 - event_set");
	while(cnt < 4) {
		printf("proc2 - event_wait(a)\n");
		rt = event_wait(e0);
		event_check_error(rt, "proc2 - event_wait");
		cnt++;
	}
	printf("proc2 - event_unset(a)\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc2 - event_unset");
	printf("proc2 - exits.\n");
	exit(0);
}

void proc3(char *event)
{
	int rt;
	int cnt = 0;
	int e0;
	printf("proc3 - event_set(a)\n");
        e0 = event_set(event);
	event_check_error_exit(e0, "proc3 - event_set");
	while (cnt < 4) {
		printf("proc3 - event_wait(a)\n");
		rt = event_wait(e0);
		event_check_error(rt, "proc3 - event_wait\n");
		cnt++;
	}
	printf("proc3 - event_unset(a)\n");
	rt = event_unset(e0);
	event_check_error_exit(e0, "proc3 - event_unset");
	printf("proc3 - exits.\n");
	exit(0);
}

int main(void)
{
	char event[2] = "a\0";
	pid_t pid;
	print_test_scenario();
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);
		proc2(event);		
		break;
	default:
		break;
	}
	pid = fork();
	switch(pid) {
	case 0:
		sleep(1);	
		proc3(event);
		break;
	default:
		break;
	}
	proc1(event);
	return 0;
}
