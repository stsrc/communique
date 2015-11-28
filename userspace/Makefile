CC = gcc

CFLAGS := -O3 -Wall -std=gnu99 -ludev
SRCFILES := $(wildcard test*.c)
OBJFILES := $(patsubst %.c, %.o, $(SRCFILES)) events.o
PROGFILES := $(patsubst %.c, %, $(SRCFILES))

.PHONY: events all clean

events: events.o

all: $(PROGFILES)
$(PROGFILES) : events.o
clean:
	rm -f $(OBJFILES) $(PROGFILES) *~
