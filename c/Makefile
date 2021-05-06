
# Run this Makefile on Red Pitaya!
# Use run.sh to upload, compile and execute.

CFLAGS  = -g -std=gnu99 -Wall -Werror
CFLAGS += -I/opt/redpitaya/include -I/opt/redpitaya/include/redpitaya
LDFLAGS = -L/opt/redpitaya/lib
LDLIBS = -lm -lpthread -lrp

CHAINFLAG ?=

OBJS=demodulation.o utility.o
EXECS=avoided_coupling_2channels.x

all: $(EXECS)

%.x: %.o $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LDLIBS)

%.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $(CHAINFLAG) $<

clean:
	$(RM) *.o
	$(RM) $(OBJS)
