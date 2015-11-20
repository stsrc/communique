#include <stdio.h>
#include <unistd.h>
#include "events.h"

int main(void)
{
	char eventa[2] = "a\0";
	char eventb[2] = "b\0";
	char eventc[2] = "c\0";
	char eventd[2] = "d\0";
	char *events[] = {eventa, eventb, eventc, eventd};
	int rt;
	pid_t pid;
	printf("proc1: event_set\n");
	rt = event_set(eventa);
	event_check_error_exit(rt, "proc1: event_set");
	rt = event_set(eventb);
	event_check_error_exit(rt, "proc1: event_set");
	rt = event_set(eventc);
	event_check_error_exit(rt, "proc1: event_set");
	rt = event_set(eventd);
	event_check_error_exit(rt, "proc1: event_set");
	pid = fork();
	switch(pid) {
	case 0:
		printf("proc2: event_set\n");
		rt = event_set(eventa);
		event_check_error_exit(rt, "proc2 - event_set FAILS!");
		sleep(3);
		printf("proc2: event_throw. eventa\n");
		rt = event_throw(eventa);
		event_check_error_exit(rt, "proc2: event_throw FAILS!");
		exit(0);
	default:
		sleep(1);
		printf("proc1: event_wait\n");
		rt = event_wait_group(events, 4);
		printf("proc1: event caught");
		event_check_error_exit(rt, "proc1: event_wait FAILS!");
		exit(0);
	}
	return 0;
}
