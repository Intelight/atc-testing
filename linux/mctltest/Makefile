CFLAGS = -O2 -W -Wall
INCLUDES = -I$(BSP_DIR)/usr/include

all:	mctltest

mctltest:	mctltest.c
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) -o $@ $<

clean:
	rm -f mctltest
