
# Run this Makefile on Red Pitaya!
# Use run.sh to upload, compile and execute.

CFLAGS  = -g -std=gnu99 -Wall
CFLAGS += -I/opt/redpitaya/include
LDFLAGS = -L/opt/redpitaya/lib
LDLIBS = -lm -lpthread -lrp

OBJS=demodulation.o
EXECS=avoided_coupling_2channels.x

all: $(EXECS)

%.x: %.o $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LDLIBS)

%.o: %.c
	$(CC) -o $@ -c $(CFLAGS) $<

clean:
	$(RM) *.o
	$(RM) $(OBJS)
