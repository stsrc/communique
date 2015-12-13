#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <errno.h>

/*
 * TEST SCENARIO:
 * proc1 waits for one of the four events: a, b, c, d
 * proc2 waits for one of the three events: a, b, d
 * proc3 tries to unset event c. Then throws c throws, and after a while throws event a.
 */

void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("All 3 processes set 4 events - a, b, c, d\n");
	printf("proc1 waits for on of four events: a, b, c, d\n");
	printf("proc2 waits for on of three events: a, b, d\n");
	printf("proc3 tries to unset event c, what is causing deadlock detection\n");
	printf("proc3 then throws event c and event a\n");
	printf("Processes unset events and exit\n\n");
	printf("Press enter to start\n");
	getchar();
	printf("\n");
}

void proc1(char **events)
{
	int rt, e0, e1, e2, e3;
	printf("proc1: event_set(e0-e3).\n");
	e0 = event_set(events[0]);
	event_check_error_exit(e0, "proc1: event_set");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc1: event_set");
	e2 = event_set(events[2]);
	event_check_error_exit(e2, "proc1: event_set");
	e3 = event_set(events[3]);
	event_check_error_exit(e3, "proc1: event_set");
	sleep(1); //assurance of waiting proc2
	printf("proc1: event_wait_group - events[0], events[1], events[2], events[3].\n");
	rt = event_wait_group(events, 4);
	event_check_error_exit(rt, "proc2: event_wait FAILED - SHOULD NOT FAIL!");
	printf("proc1: event_unset(e0-e3).\n");
	rt = event_unset(e0);
	event_check_error(rt, "proc1: event_unset event0");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc1: event_unset event1");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc1: event_unset event2");
	rt = event_unset(e3);
	event_check_error_exit(rt, "proc1: event_unset event3");
	printf("proc1: exits.\n");
	sleep(1);
	exit(0);
}

void proc2(char **events)
{
	int rt, e0, e1, e2, e3;
	char *events_2[] = {events[0], events[1], events[3]};
	printf("proc2: event_set(e0-e3).\n");
	e0 = event_set(events[0]);
	event_check_error_exit(e0, "proc2: event_set");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc2: event_set");
	e2 = event_set(events[2]);
	event_check_error_exit(e2, "proc2: event_set");
	e3 = event_set(events[3]);
	event_check_error_exit(e3, "proc2: event_set");	
	printf("proc2: event_wait_group - events[0], events[1], events[3].\n");
	rt = event_wait_group(events_2, 3);
	event_check_error_exit(rt, "proc2: event_wait_group. SHOULD NOT FAIL");
	printf("proc2: event_unset(e0-e3).\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc2: event_unset");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc2: event_unset");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc2: event_unset");
	rt = event_unset(e3);
	event_check_error_exit(rt, "proc2: event_unset");
	printf("proc2: exits.\n");
	exit(0);
}

void proc3(char **events)
{
	int rt, e0, e1, e2, e3;
	printf("proc3: event_set(e0-e3).\n");
	e0 = event_set(events[0]);
	event_check_error_exit(e0, "proc3: event_set");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc3: event_set");
	e2 = event_set(events[2]);
	event_check_error_exit(e2, "proc3: event_set");
	e3 = event_set(events[3]);
	event_check_error_exit(e3, "proc3: event_set");	
	sleep(3);
	printf("proc3: event_unset - events[2]. Should fail.\n");
	rt = event_unset(e2);
	event_check_error(rt, "proc3: event_unset. Positive fail");
	printf("proc3: event_throw - events[2].\n");
	rt = event_throw(e2);
	event_check_error_exit(rt, "proc3: event_throw");
	printf("proc3: event_unset - events[2].\n");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc3: event_unset");
	sleep(1); 
	printf("proc3: event_throw - events[0].\n");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc3: event_throw");
	printf("proc3: event_unset(e0 e1 e3).\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc3: event_unset");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc3: event_unset");
	rt = event_unset(e3);
	event_check_error_exit(rt, "proc3: event_unset");
	printf("proc3: exits.\n");
	exit(0);
}

int main(void)
{
	char eventa[2] = "a\0";
	char eventb[2] = "b\0";
	char eventc[2] = "c\0";
	char eventd[2] = "d\0";
	char *events[] = {eventa, eventb, eventc, eventd};
	pid_t pid;
	print_test_scenario();
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
