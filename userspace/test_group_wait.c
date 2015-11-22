#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <errno.h>

/*
 * TEST SCENARIO:
 * first process throws one of the four event,
 * second waits in group_wait
 */

int main(void)
{
	int cnt = 0;
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
		while(cnt < 4) {
			printf("proc2: event_wait\n");
			rt = event_wait_group(events, 4);
			printf("proc2: event caught\n");
			event_check_error_exit(rt, "proc2: event_wait FAILS!");
			cnt++;
		}
		printf("proc2: exiting\n");
		exit(0);
	default:
		sleep(1);
		while(cnt < 4) {
			printf("proc1: event_throw, event %s\n", events[cnt]);
			rt = event_throw(events[cnt]);
			event_check_error_exit(rt, "proc2: event_throw FAILS!");
			sleep(2);
			cnt++;
		}
		sleep(5);
		printf("proc1: Unsetting events\n");
		rt = event_unset(events[0]);
		event_check_error(rt, "proc1: event_unset event0");
		rt = event_unset(events[1]);
		event_check_error(rt, "proc1: event_unset event1");
		rt = event_unset(events[2]);
		event_check_error(rt, "proc1: event_unset event2");
		rt = event_unset(events[3]);
		event_check_error(rt, "proc1: event_unset event3");
		printf("proc1: exiting\n");
		exit(0);
	}
	return 0;
}
