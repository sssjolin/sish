CFLAGS	+= -Wall
LDFLAGS	+=

OBJS	= main.o util.o 

sish: 	$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

clean:
	rm -f sish $(OBJS)
