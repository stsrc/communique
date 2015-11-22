#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <errno.h>

/*
 * TEST SCENARIO:
 * proc1 waits for one of the four events: a, b, c, d
 * proc2 waits for one of the three events: a, b, d
 * proc3 just throws. Firstly throws event c (proc1 kill), then throws event (a)
 *	(proc2 kill), and then makes suicide (proc3 kill).
 */

void proc1(char **events)
{
	int rt;
	sleep(1); //assurance of waiting proc2
	printf("proc1: event_wait_group\n");
	rt = event_wait_group(events, 4);
	event_check_error_exit(rt, "proc2: event_wait FAILED - SHOULD NOT FAIL!");
	rt = event_unset(events[0]);
	event_check_error(rt, "proc1: event_unset event0");
	rt = event_unset(events[1]);
	event_check_error(rt, "proc1: event_unset event1");
	rt = event_unset(events[2]);
	event_check_error(rt, "proc1: event_unset event2");
	rt = event_unset(events[3]);
	event_check_error(rt, "proc1: event_unset event3");
	printf("proc1 kill\n");
	exit(0);
}

void proc2(char **events)
{
	int rt;
	char *events_2[] = {events[0], events[1], events[3]};
	rt = event_set(events[0]);
	event_check_error_exit(rt, "proc2: event_set");
	rt = event_set(events[1]);
	event_check_error_exit(rt, "proc2: event_set");
	rt = event_set(events[2]);
	event_check_error_exit(rt, "proc2: event_set");
	rt = event_set(events[3]);
	event_check_error_exit(rt, "proc2: event_set");	
	printf("proc2: event_wait_group.\n");
	rt = event_wait_group(events_2, 3);
	event_check_error_exit(rt, "proc2: event_wait_group. SHOULD NOT FAIL");
	rt = event_unset(events[0]);
	event_check_error_exit(rt, "proc2: event_unset");
	rt = event_unset(events[1]);
	event_check_error_exit(rt, "proc2: event_set");
	rt = event_unset(events[2]);
	event_check_error_exit(rt, "proc2: event_unset");
	rt = event_unset(events[3]);
	event_check_error_exit(rt, "proc2: event_unset");
	printf("proc2 kill\n");
	exit(0);
}

void proc3(char **events)
{
	int rt;
	rt = event_set(events[0]);
	event_check_error_exit(rt, "proc3: event_set");
	rt = event_set(events[1]);
	event_check_error_exit(rt, "proc3: event_set");
	rt = event_set(events[2]);
	event_check_error_exit(rt, "proc3: event_set");
	rt = event_set(events[3]);
	event_check_error_exit(rt, "proc3: event_set");	
	sleep(5); //assurance of waiting proc1 
	rt = event_throw(events[2]);
	event_check_error_exit(rt, "proc3: event_throw");
	sleep(1); //?
	rt = event_throw(events[0]);
	event_check_error_exit(rt, "proc3: event_throw");
	rt = event_unset(events[0]);
	event_check_error_exit(rt, "proc3: event_unset");
	rt = event_unset(events[1]);
	event_check_error_exit(rt, "proc3: event_unset");
	rt = event_unset(events[2]);
	event_check_error_exit(rt, "proc3: event_unset");
	rt = event_unset(events[3]);
	event_check_error_exit(rt, "proc3: event_unset");
	printf("proc3 kill\n");
	exit(0);
}

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
		proc2(events);
	default:
		break;
	}
	pid = fork();
	switch(pid) {
	case 0:
		proc3(events);
	default:
		break;
	}
	proc1(events);
	return 0;
}
