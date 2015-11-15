#include <stdio.h>
#include <unistd.h>
#include "events.h"

int main(void) {
	int rt;
	char event = 'a';
	printf("Proc2 entered\n");
	while(1) {
		sleep(1);
		printf("Proc2 sleeps on event\n");
		rt = event_wait(event);
		event_check_error(rt, "event_wait");
	}	
	return 0;
}
