#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <semaphore.h>
#include <sys/sem.h>

/*
 * TEST SCENARIO:
 * 1. proc1 sets event a and event b;
 * 2. proc2 sets a;
 * 3. proc2 waits for a;
 * 4. proc3 sets event a and b;
 * 5. proc1 waits for event b;
 * 6. proc3 tries to unset event a, should FAIL;
 * 7. proc3 throws event b;
 * 8. proc3 tries to unset event a, should NOT FAIL;
 * 9. proc1 throws event a;
 * 10. proc1 tries to unset event a, should NOT FAIL;
 * 11. proc2 unsets event a;
 * 12. processes exits;
 */
void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("1. proc1 sets event a and event b\n");
	printf("2. proc2 sets event a\n");
	printf("3. proc2 waits for event a\n");
	printf("4. proc3 sets event a and b\n");
	printf("5. proc1 waits for event b\n\n");
	printf("--> proc3 sleeps for a while to guarantee that proc1 and proc2"
	       " have ended above steps <--\n\n");
	printf("6. proc3 tries to unset event a, should fail - driver "
	       "throws deadlock error\n");
	printf("7. proc3 throws event b\n");
	printf("8. proc3 unsets event a and b\n");
	printf("9. proc1 throws event a\n");
	printf("10. proc1 unsets event a and b\n");
	printf("11. proc2 unsets event a\n");
	printf("12. Processes exits\n\n");
	printf("To start press any button\n");
	getchar();
	printf("\n");
}

void proc1()
{
	int e0, e1, rt;
	char *eventa = "a\0";
	char *eventb = "b\0";
	printf("1. proc1 - event_set\n");
	e0 = event_set(eventa);
	event_check_error_exit(e0, "proc1 - event_set");
	e1 = event_set(eventb);
	event_check_error_exit(e1, "proc1 - event_set");
	sleep(4);
	printf("5. proc1 - event_wait\n");
	rt = event_wait(e1);
	event_check_error_exit(rt, "proc1 - event_wait");
	printf("9. proc1 - event_throw\n");
	rt = event_throw(e0);
	event_check_error_exit(rt, "proc1 - event_throw");
	printf("10. proc1 - event_unset\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc1 - event_unset");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc1 - event_unset");
	printf("12. proc1 - exit(0)\n");
	sleep(1);
	exit(0);
}

void proc2()
{
	char *eventa = "a\0";
	int rt, e0;
	sleep(1);
	printf("2. proc2 - event_set\n");
	e0 = event_set(eventa);
	event_check_error_exit(e0, "proc2 - event_set");
	printf("3. proc2 - event_wait\n");
	rt = event_wait(e0);
	event_check_error_exit(rt, "proc2 - event_wait");
	printf("11. proc2 - event_unset\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc2 - event_unset");
	printf("12. proc2 - exit(0)\n");
	exit(0);	
}

void proc3()
{
	int rt, e0, e1;
	char *eventa = "a\0";
	char *eventb = "b\0";
	sleep(2);
	printf("4. proc3 - event_set\n");
	e0 = event_set(eventa);
	event_check_error_exit(e0, "proc3 - event_set");
	e1 = event_set(eventb);
	event_check_error_exit(e1, "proc3 - event_set");
	sleep(10);
	printf("6. proc3 - event_unset, should fail\n");
	rt = event_unset(e0);
	event_check_error(rt, "proc3 - event_unset, positive fail");
	printf("7. proc3 - event_throw\n");
	rt = event_throw(e1);
	event_check_error_exit(rt, "proc3 - event_throw");
	printf("8. proc3 - event_unset\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "proc3 - event_unset");
	rt = event_unset(e1);
	event_check_error_exit(rt, "proc3 - event_unset");
	printf("12. proc3 - exit(0)\n");
	exit(0);
}

int main(void)
{

	pid_t pid;
	print_test_scenario();
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
