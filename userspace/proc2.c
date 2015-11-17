#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "events.h"

int main(void) {
	int rt;
	int cnt = 0;
	char event[2] = "a\0";
	char false_event[2] = "b\0";
	if (event_set(event)) {
		printf("proc2 sees that true event was made.\n");
	} else {
		printf("proc2 does not see that true event exists!\n");
		return 1;
	}
	rt = event_set(false_event);
	event_check_error(rt, "event_set");
	printf("proc2 entered\n");
	while(cnt < 4) {
		sleep(1);
		printf("proc2 sleeps on event.\n");
		rt = event_wait(event);
		printf("proc2 awaken.\n");
		event_check_error(rt, "event_wait");
		cnt++;
	}
	event_unset(false_event);
	event_unset(event);	
	return 0;
}
