/*
 * mctltest.c
 *
 * Copyright (C) 2017 Intelight Inc. <mike.gallagher@intelight-its.com>
 *
 * A low-level serial port external loopback modem control signal test.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

struct termios old_termios;

static void usage(void) __attribute__ ((__noreturn__));

static void usage(void)
{
	fprintf(stderr, "mctltest version 1.0\n"
		"\n"
		"Usage: mctltest (port1 | port1:port2)");
	exit(1);
}

void port_config_async(int fd)
{
        // set serial port to raw mode, set baudrate
        struct termios new_termios;
         
        if (tcgetattr(fd, &old_termios) < 0) {
                fprintf(stderr, "port_config tcgetattr error %s\n", strerror(errno));
                exit(1);
        }
    
        memcpy (&new_termios, &old_termios, sizeof(struct termios)); 
   	new_termios.c_cflag = B9600|CS8|CLOCAL|CREAD;
        new_termios.c_iflag = IGNBRK|IGNPAR;
        new_termios.c_oflag = 0;
        new_termios.c_lflag = 0;
        new_termios.c_cc[VTIME] = 0;
        new_termios.c_cc[VMIN] = 1;

        tcflush(fd,TCIFLUSH);
        if (tcsetattr(fd, TCSANOW, &new_termios) < 0) {
                fprintf(stderr, "port_config tcsetattr error %s\n", strerror(errno));
                exit(1);
        }
}

void port_restore(int fd)
{
        tcflush(fd, TCIFLUSH);
        if (tcsetattr(fd, TCSANOW, &old_termios) < 0) {
                fprintf(stderr, "port tcsetattr error %s\n", strerror(errno));
                exit(1);
        }
}

void mctl_test(char *port1, char *port2)
{
        int exitcode = 0;
	int tx_fd, rx_fd;
        int mctrl = 0;
	struct timespec delay = {0, 20000000};

        if ((tx_fd = open(port1, O_RDWR)) < 0) {
                fprintf(stderr, "Could not open serial port %s error %s\n",
                        port1, strerror(errno));
                exit(1);
        }
        port_config_async(tx_fd);
        
        if (strcmp(port1, port2) != 0) {
                if ((rx_fd = open(port2, O_RDWR)) < 0) {
                        fprintf(stderr, "Could not open serial port %s error %s\n",
                                port2, strerror(errno));
                        exit(1);
                }
                port_config_async(rx_fd);
        } else {
                rx_fd = tx_fd;
        }


        // Assert RTS on port 1
        if (ioctl(tx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port1, strerror(errno));
                goto error_exit;
        }
        mctrl |= TIOCM_RTS;
        if (ioctl(tx_fd, TIOCMSET, &mctrl) != 0) {
                fprintf(stderr, "Could not set mctrl state, port %s, error %s\n",
                                port1, strerror(errno));
                goto error_exit;
        }
        nanosleep(&delay, NULL);
        if (ioctl(tx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port1, strerror(errno));
                goto error_exit;
        }
        if ((mctrl&TIOCM_RTS) == 0) {
                fprintf(stderr, "RTS failed to Assert, port %s\n",
                                port1);
                goto error_exit;
        }

        // Check CTS/DCD asserted on port 2
        if (ioctl(rx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port2, strerror(errno));
                goto error_exit;
        }
        if ((mctrl&TIOCM_CTS) == 0) {
                fprintf(stderr, "CTS failed to assert, port %s\n",
                                port2);
                goto error_exit;
        }

        if ((mctrl&TIOCM_CD) == 0) {
                fprintf(stderr, "DCD failed to assert, port %s\n",
                                port2);
                goto error_exit;
        }

        // De-assert RTS on port 1
        if (ioctl(tx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port1, strerror(errno));
                goto error_exit;
        }
        mctrl &= ~TIOCM_RTS;
        if (ioctl(tx_fd, TIOCMSET, &mctrl) != 0) {
                fprintf(stderr, "Could not set mctrl state, port %s, error %s\n",
                                port1, strerror(errno));
                goto error_exit;
        }
        nanosleep(&delay, NULL);
        if (ioctl(tx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port1, strerror(errno));
                goto error_exit;
        }
        if ((mctrl&TIOCM_RTS) != 0) {
                fprintf(stderr, "RTS failed to de-assert, port %s\n",
                                port1);
                goto error_exit;
        }

        // Check CTS/DCD de-asserted on port 2
        if (ioctl(rx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port2, strerror(errno));
                goto error_exit;
        }
        if ((mctrl&TIOCM_CTS) != 0) {
                fprintf(stderr, "CTS failed to de-assert, port %s\n",
                                port2);
                goto error_exit;
        }
#if 1
        if ((mctrl&TIOCM_CD) != 0) {
                fprintf(stderr, "DCD failed to de-assert, port %s\n",
                                port2);
                goto error_exit;
        }
#endif

        // Assert RTS on port 2
        if (ioctl(rx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port2, strerror(errno));
                goto error_exit;
        }
        mctrl |= TIOCM_RTS;
        if (ioctl(rx_fd, TIOCMSET, &mctrl) != 0) {
                fprintf(stderr, "Could not set mctrl state, port %s, error %s\n",
                                port2, strerror(errno));
                goto error_exit;
        }
        nanosleep(&delay, NULL);
        if (ioctl(rx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port2, strerror(errno));
                goto error_exit;
        }
        if ((mctrl&TIOCM_RTS) == 0) {
                fprintf(stderr, "RTS failed to Assert, port %s\n",
                                port2);
                goto error_exit;
        }

        // Check CTS/DCD asserted on port 1
        if (ioctl(tx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port1, strerror(errno));
                goto error_exit;
        }
        if ((mctrl&TIOCM_CTS) == 0) {
                fprintf(stderr, "CTS failed to assert, port %s\n",
                                port1);
                goto error_exit;
        }
        if ((mctrl&TIOCM_CD) == 0) {
                fprintf(stderr, "DCD failed to assert, port %s\n",
                                port1);
                goto error_exit;
        }
        

        // De-assert RTS on port 2
        if (ioctl(rx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port2, strerror(errno));
                goto error_exit;
        }
        mctrl &= ~TIOCM_RTS;
        if (ioctl(rx_fd, TIOCMSET, &mctrl) != 0) {
                fprintf(stderr, "Could not set mctrl state, port %s, error %s\n",
                                port2, strerror(errno));
                goto error_exit;
        }
        nanosleep(&delay, NULL);
        if (ioctl(rx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port2, strerror(errno));
                goto error_exit;
        }
        if ((mctrl&TIOCM_RTS) != 0) {
                fprintf(stderr, "RTS failed to de-assert, port %s\n",
                                port2);
                goto error_exit;
        }

        // Check CTS/DCD de-asserted on port 1
        if (ioctl(tx_fd, TIOCMGET, &mctrl) != 0) {
                fprintf(stderr, "Could not get mctrl state, port %s, error %s\n",
                                port1, strerror(errno));
                goto error_exit;
        }
        if ((mctrl&TIOCM_CTS) != 0) {
                fprintf(stderr, "CTS failed to de-assert, port %s\n",
                                port1);
                goto error_exit;
        }
#if 1
        if ((mctrl&TIOCM_CD) != 0) {
                fprintf(stderr, "DCD failed to de-assert, port %s\n",
                                port1);
                goto error_exit;
        }
#endif

        printf("Modem Control Signal Test %s:%s Passed\n", port1, port2);

        goto cleanup;

error_exit:
        exitcode = 1;

cleanup:
        port_restore(tx_fd);
	close(tx_fd);

        if (strcmp(port1, port2) != 0) {
                port_restore(rx_fd);
                close(rx_fd);
        }
        
        exit(exitcode);

}

int main(int argc, char *argv[])
{
        char *port1, *port2;
        
	if (argc != 2)
		usage();

	port1 = argv[1];
	if ((port2 = strchr(argv[1], ':')))
		*(port2++) = '\x0';

	if (port1[0] == '\x0') {
		fprintf(stderr, "Empty serial port name\n");
                exit(1);
        }

	if (port2) {
		if (port2[0] == '\x0') {
			fprintf(stderr, "Empty serial port name\n");
                        exit(1);
                }
        }
        
	mctl_test(port1, port2 ? port2 : port1);

	exit(0);
}
