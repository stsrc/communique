#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <errno.h>

/*
 * TEST SCENARIO:
 * proc1 waits for one of the four events: a, b, c, d. Throws the same.
 * proc2 waits for one of the four events: a, b, c, d. 
 * proc1 waits after proc2. So proc1 should fail (deadlock error occures)
 */

void proc1(char **events)
{
	int rt;
	sleep(2); //assurance of proc2 being in waiting state
	printf("proc1: event_wait_group\n");
	rt = event_wait_group(events, 4);
	event_check_error_exit(rt, "proc1: event_wait FAILED - SHOULD FAIL!");
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
	rt = event_set(events[0]);
	event_check_error_exit(rt, "proc2: event_set");
	rt = event_set(events[1]);
	event_check_error_exit(rt, "proc2: event_set");
	rt = event_set(events[2]);
	event_check_error_exit(rt, "proc2: event_set");
	rt = event_set(events[3]);
	event_check_error_exit(rt, "proc2: event_set");	
	printf("proc2: event_wait_group.\n");
	rt = event_wait_group(events, 4);
	event_check_error_exit(rt, "proc2: event_wait_group. SHOULD NOT FAIL");
	printf("proc2: not failing from deadlock!\n");
	exit(1);
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
	proc1(events);
	return 0;
}
