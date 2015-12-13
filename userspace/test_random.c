#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <stdlib.h>
#include <time.h>

void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("process 1 - sends event \"a\".\n");
	printf("process 2 - waits for event \"a\".\n");
	printf("process 3 - sets event \"a\", throws event, waits for "
	       "event and unsets event in one loop iteration.\n\n");
	printf("To exit program hit ctrl+c. " 
	       "Driver should still be stable after program termination.\n\n");
	printf("To start program press enter.\n");
	getchar();
	printf("\n\n");
}

void proc1(char *event)
{
	int rt = 0;
	int eid = event_set(event);
	event_check_error(eid, "main: event_set");
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = 20000000 + rand()%4000001;
		nanosleep(&timespec, NULL);
		printf("proc1: event_throw.\n");
		rt = event_throw(eid);
		if (rt == 1)
			continue;
		event_check_error_exit(rt, "proc1 - event_throw");
	}
	printf("proc1 - event unset.\n");
	rt = event_unset(eid);
	event_check_error_exit(rt, "proc1 - event_unset");
	exit(0);
}

void proc2(char *event)
{
	int rt;
	int eid = event_set(event);
	event_check_error(eid, "proc2 - event_set");
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = 20000000 + rand()%1000001;
		nanosleep(&timespec, NULL);
		printf("proc2: event_wait.\n");	
		rt = event_wait(eid);
		event_check_error_exit(rt, "proc2 - event_wait");
	}
	exit(0);
}

void proc3(char *event)
{
	int rt, eid;
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = 20000000 + rand()%1001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_set.\n");
		eid = event_set(event);
		if (eid < 0) {
			while(1) { perror("proc3: event_set"); }
		}
		event_check_error_exit(rt, "proc3: event_set\n");
		timespec.tv_nsec = 20000000 + rand()%1001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_wait.\n");
		rt = event_wait(eid);
		event_check_error_exit(rt, "proc3: event_wait\n");
		timespec.tv_nsec = rand()%1001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_throw.\n");
		rt = event_throw(eid);
		event_check_error_exit(rt, "proc3: event_throw\n");
		timespec.tv_nsec = 20000000 + rand()%1001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_unset.\n");
		rt = event_unset(eid);
		if (rt) {
			while(1) {
				perror("proc3: event_unset");
			}
		}
		event_check_error_exit(rt, "proc3: event_unset\n");
	}
}

int main(void)
{
	char event[2] = "a\0";
	pid_t pid;
	print_test_scenario();
	pid = fork();
	switch(pid) {
	case 0:
		proc2(event);	
		break;
	default:
		break;
	}
	pid = fork();
	switch(pid) {
	case 0:
		proc3(event);		
		break;
	default:
		break;
	}
	proc1(event);
	return 0;
}
