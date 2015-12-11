#ifndef _EVENTS_H_
#define _EVENTS_H_
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libudev.h>
#include <asm-generic/ioctl.h>

int event_set(char *name);
int event_wait(int event_num);
int event_wait_group(char **events, int events_cnt);
int event_throw(int event_num);
int event_unset(int event_num);
int event_check_error_exit(int rt, char *st);
int event_check_error(int rt, char *st);

#endif //_EVENTS_H_
