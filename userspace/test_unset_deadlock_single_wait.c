#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <semaphore.h>


#include <sys/sem.h>

/*
 * TEST SCENARIO:
 * 1. proc1 sets event a;
 * 2. proc2 waits for event a;
 * 3. proc1 tries to unset event a, should FAIL;
 *
 * not implemented.
 *
 * 4. proc3 sets event a and b;
 * 5. proc1 waits for event b;
 * 6. proc3 tries to unset event a, should FAIL;
 * 7. proc3 throws event b;
 * 8. proc3 tries to unset event a, should NOT FAIL;
 * 9. proc1 throws event a;
 * 10. proc1 tries to unset event a, should NOT FAIL;
 * 11. proc2 dead;
 */


void proc1()
{
	int rt;
	char *eventa = "a\0";
	char *eventb = "b\0";
	printf("1. proc1 - event_set\n");
	rt = event_set(eventa);
	event_check_error_exit(rt, "proc1 - event_set, FAILED - SHOULD NOT FAIL!");
	sleep(4);
	printf("5. proc1 - event_wait\n");
	rt = event_wait(eventb);
	event_check_error_exit(rt, "proc1 - event_wait, FAILED - SHOULD NOT FAIL!\n");
	printf("9. proc1 - event_throw\n");
	rt = event_throw(eventa);
	event_check_error_exit(rt, "proc1 - event_throw, FAILED - SHOULD NOT FAIL!\n");
	printf("10. proc1 - event_unset\n");
	rt = event_unset(eventa);
	event_check_error_exit(rt, "proc1 - event_unset, FAILED - SHOULD NOT FAIL!");
	exit(0);
}

void proc2()
{
	char *eventa = "a\0";
	int rt;
	sleep(1);
	printf("2. proc2 - event_wait\n");
	rt = event_wait(eventa);
	event_check_error_exit(rt, "proc2 - event_wait, FAILED - SHOULD NOT FAIL!");
	printf("11. proc2 - dead\n");
	exit(0);	
}

void proc3()
{
	int rt;
	char *eventa = "a\0";
	char *eventb = "b\0";
	sleep(2);
	printf("4. proc3 - event_set\n");
	rt = event_set(eventa);
	event_check_error_exit(rt, "proc3 - event_set, FAILED - SHOULD NOT FAIL!");
	rt = event_set(eventb);
	event_check_error_exit(rt, "proc3 - event_set, FAILED - SHOULD NOT FAIL!");
	sleep(10);
	printf("6. proc3 - event_unset, should fail\n");
	rt = event_unset(eventa);
	event_check_error(rt, "proc3 - event_unset, positive fail\n");
	printf("7. proc3 - event_throw\n");
	rt = event_throw(eventb);
	event_check_error_exit(rt, "proc3 - event_throw, FAILED - SHOULD NOT FAIL!");
	printf("8. proc3 - event_unset\n");
	rt = event_unset(eventa);
	event_check_error_exit(rt, "proc3 - event_unset, FAILED - SHOULD NOT FAIL!");
	rt = event_unset(eventb);
	event_check_error_exit(rt, "proc3 - event_unset, FAILED - SHOULD NOT FAIL!");
	exit(0);
}

int main(void)
{

	pid_t pid;
	pid = fork();
	switch(pid) {
	case 0:
		proc2();
		break;
	default:
		break;
	}
	pid = fork();
	switch(pid) {
	case 0:
		proc3();
		break;
	default:
		break;
	}
	proc1();
	return 0;
}
