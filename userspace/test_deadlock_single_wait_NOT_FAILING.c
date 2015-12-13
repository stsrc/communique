#include <stdio.h>
#include <unistd.h>
#include "events.h"

void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("proc2: sets events: \"a\", \"b\". Firstly waits for event"
	       " \"a\", then throws event \"b\" and exits.\n");
	printf("proc3: sets events: \"b\", \"c\". Process waits for event"
	       " \"b\" and exits.\n");
	printf("proc4: sets event \"c\". Process throws event \"c\" and exits.\n");
	printf("proc1: sets events: \"a\", \"c\". Firstly waits for event "
	       "\"c\", secondly throws event \"a\", thirdly exits.\n");
	printf("On exit all processes unset related events.\n\n");
	printf("To start press enter.\n");
	getchar();
}

void proc2(char **events)
{
	int rt, e0, e1;
	printf("proc2 - event_set(a and b)\n");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc2 - event_set");
	e0 = event_set(events[0]);
	event_check_error_exit(e0, "proc2 - event_set");
	printf("proc2 - event_wait(a)\n");
	rt = event_wait(e0);
	event_check_error_exit(rt, "proc2 - event_wait");
	printf("proc2 - event_throw(b)\n");
	rt = event_throw(e1);
	event_check_error_exit(rt, "proc2 - event_throw");
	printf("proc2 - event_unset(a and b)\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc2 - event_unset");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc2 - event_unset");
	printf("proc2 - exits\n");
	exit(0);	
}

void proc3(char **events)
{
	int rt, e1, e2;
	printf("proc3 - event_set(b and c)\n");
	e2 = event_set(events[2]);
	event_check_error_exit(e2, "proc3 - event_set");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc3 - event_set");
	printf("proc3 - event_wait(b)\n");
	rt = event_wait(e1);
	event_check_error_exit(rt, "proc3 - event_wait");
	printf("proc3 - event_unset(b and c)\n");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc3 - event_unset");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc3 - event_unset");
	printf("proc3 - exits\n");
	exit(0);	
}

void proc4(char **events)
{
	int rt, e2;
	printf("proc4 - event_set(a)\n");
	e2 = event_set(events[2]);
	event_check_error_exit(e2, "proc4 - event_set");
	sleep(3);
	printf("proc4 - event_throw(a)\n");
	rt = event_throw(e2);
	event_check_error_exit(rt, "proc4 - event_throw");
	printf("proc4 - event_unset(a)\n");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc4 - event_unset");
	printf("proc4 - exits\n");
	exit(0);
}

int main(void)
{
	char *events[] = {"a\0", "b\0", "c\0"};
	pid_t pid;
	int rt, e0, e2;
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
	pid = fork();
	switch (pid) {
	case 0:
		sleep(3);
		proc4(events);
		break;
	default:
		break;
	}
	printf("proc1 - event_set(a and c)\n");
	e0 = event_set(events[0]);
	event_check_error_exit(e0, "proc1 - event_set");
	e2 = event_set(events[2]);
	event_check_error_exit(e2, "proc1 - event_set");
	sleep(5);
	printf("proc1 - event_wait(c)\n");
	rt = event_wait(e2);
	event_check_error_exit(rt, "proc1 - event_wait");
	printf("proc1 - event_throw(a)\n");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc1 - event_throw");
	printf("proc1 - event_unset(a and c)\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1 - event_unset");	
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("proc1 - exits\n");
	sleep(1);
	return 0;
}
