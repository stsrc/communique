CC = gcc

CFLAGS := -O3 -Wall -std=gnu99
SRCFILES := $(wildcard test*.c)
OBJFILES := $(patsubst %.c, %.o, $(SRCFILES)) events.o
PROGFILES := $(patsubst %.c, %, $(SRCFILES))

.PHONY: events all clean

all: $(PROGFILES)
$(PROGFILES) : events.o

events: events.o

clean:
	rm -f $(OBJFILES) $(PROGFILES) *~
