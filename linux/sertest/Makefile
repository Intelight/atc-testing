CFLAGS = -O2 -W -Wall
INCLUDES = -I$(BSP_DIR)/usr/include

all:	sertest

sertest:	sertest.c
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) -o $@ $<

clean:
	rm -f sertest
