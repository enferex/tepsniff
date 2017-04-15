CC=gcc
CFLAGS=-g3 -O0 -pedantic -Wall $(EXTRA_CFLAGS)
SRCS=main.c
OBJS=$(SRCS:.c=.o)
APP=tepsniff

all: $(APP)

%.o: %.c
	$(CC) -c $^ $(CFLAGS) -o $@

$(APP): $(OBJS)
	$(CC) $^ -o $@

clean:
	$(RM) -v $(APP) $(OBJS)
