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

void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("proc1 sets four events: \"a\", \"b\", \"c\", \"d\".\n");
	printf("proc2 sets the same events.\n");
	printf("proc2 waits for one of four events as a first. When proc2 sleeps, "
	       "proc1 tries to wait in the same way, what causes deadlock\n");
	printf("proc1 event_wait should return error about deadlock.\n\n");
	printf("Press enter to start.\n");
	getchar();
}
void proc1(char **events)
{
	int rt, e0, e1, e2, e3;
	printf("proc1: event_set a\n");
	e0 = event_set(events[0]);
	event_check_error_exit(e0, "proc1: event_set");
	printf("proc1: event_set b\n");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc1: event_set");
	printf("proc1: event_set c\n");
	e2 = event_set(events[2]);
	event_check_error_exit(e2, "proc1: event_set");
	printf("proc1: event_set d\n");
	e3 = event_set(events[3]);
	event_check_error_exit(e3, "proc1: event_set");
	sleep(2); //assurance of proc2 being in waiting state
	printf("proc1: event_wait_group - should fail\n");
	rt = event_wait_group(events, 4);
	event_check_error(rt, "proc1: event_wait, positive fail");
	printf("proc1: event_throw a\n");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc1: event_throw event0");
	printf("proc1: event_unset a\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1: event_unset event0");
	printf("proc1: event_unset b\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc1: event_unset event1");
	printf("proc1: event_unset c\n");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc1: event_unset event2");
	printf("proc1: event_unset d\n");
	rt = event_unset(e3);
	event_check_error_exit(rt, "proc1: event_unset event3");
	sleep(1);
	printf("proc1 exits\n");
	exit(0);
}

void proc2(char **events)
{
	int rt, e0, e1, e2, e3;
	printf("proc2: event_set a\n");
	e0 = event_set(events[0]);
	event_check_error_exit(e0, "proc2: event_set");
	printf("proc2: event_set b\n");
	e1 = event_set(events[1]);
	event_check_error_exit(e1, "proc2: event_set");
	printf("proc2: event_set c\n");
	e2 = event_set(events[2]);
	event_check_error_exit(e2, "proc2: event_set");
	printf("proc2: event_set d\n");
	e3 = event_set(events[3]);
	event_check_error_exit(e3, "proc2: event_set");	
	printf("proc2: event_wait_group\n");
	rt = event_wait_group(events, 4);
	event_check_error_exit(rt, "proc2: event_wait_group. SHOULD NOT FAIL");
	printf("proc2: event_unset a\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc2: event_unset");
	printf("proc2: event_unset b\n");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc2: event_unset");
	printf("proc2: event_unset c\n");
	rt = event_unset(e2);
	event_check_error_exit(rt, "proc2: event_unset");
	printf("proc2: event_unset d\n");
	rt = event_unset(e3);
	event_check_error_exit(rt, "proc2: event_unset");
	printf("proc2 exits\n");
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
	proc1(events);
	return 0;
}
