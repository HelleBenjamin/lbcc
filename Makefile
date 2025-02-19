CFLAGS=-Wall -std=c11 -g
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

lbcc: $(OBJS)
	gcc -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): lbcc.h

clean:
	rm -f lbcc *.o *~ tmp* a.out test/*~

format:
	clang-format -i *.c *.h

.PHONY: clean format
