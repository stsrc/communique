#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "events.h"

int main(void) {
	int rt;
	int cnt = 0;
	char event[2] = "a\0";
	char false_event[2] = "b\0";
	rt = event_set(false_event);
	event_check_error(rt, "event_set");
	printf("Proc2 entered\n");
	while(cnt < 4) {
		sleep(1);
		printf("proc2 sleeps on event.\n");
		rt = event_wait(event);
		printf("proc2 awaken.\n");
		event_check_error(rt, "event_wait");
		cnt++;
	}	
	return 0;
}
