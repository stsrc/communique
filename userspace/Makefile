CC = gcc
LD = gcc

CFLAGS := -O3 -Wall -std=gnu99

ALL := proc1 proc2

.PHONY: clean proc1 proc2

OBJS := events.o

OBJS1 := $(OBJS) proc1.o
OBJS2 := $(OBJS) proc2.o 

proc1 : $(OBJS1)
	$(LD) $(OBJS1) -o $@

proc2 : $(OBJS2)
	$(LD) $(OBJS2) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ALL) $(OBJS)
