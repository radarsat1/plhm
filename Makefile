
SOURCES=libertyd.c OSC-client.c OSC-timetag.c
HEADERS=OSC-client.h OSC-timetag.h
OBJS=$(SOURCES:%.c=%.o)
TARGET=libertyd
LIBS=-llo

ifdef DEBUG
CFLAGS=-ggdb -Wall -DDEBUG
else
CFLAGS=-O3 -Wall
endif

$(TARGET): $(OBJS)
	g++ $(LDFLAGS) -o $@ $^ $(LIBS)

%.o: %.c $(HEADERS)
	g++ $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	-rm $(OBJS) $(TARGET) *~
