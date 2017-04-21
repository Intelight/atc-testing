/*
 * sertest.c
 *
 * Copyright (C) 2017 Intelight Inc. <mike.gallagher@intelight-its.com>
 *
 * A low-level serial port external loopback test.
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
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include <atc_spxs.h>


struct termios old_termios;

static void usage(void) __attribute__ ((__noreturn__));

static void usage(void)
{
	fprintf(stderr, "sertest version 1.0\n"
		"\n"
		"Usage: sertest (port1 | port1:port2)"
		" [port speed [number_of_packets [packet_size]]]\n");
	exit(1);
}


int tx(int tx_fd, unsigned char *buffer, int packet_size, int timeout)
{
        struct pollfd pfd;
	ssize_t len = 0, count = 0;

        do {
                pfd.fd = tx_fd;
                pfd.events = POLLOUT;
        
                switch(poll(&pfd, 1, timeout)) {
                case 0:
                        return 0;
                case -1:
                        fprintf(stderr, "poll() failed: %s\n", strerror(errno));
                        exit(1);
                default:
                        break;
                }
                len = write(tx_fd, &buffer[count], packet_size - count);
                if (len < 0) {
                        if (errno == EWOULDBLOCK) {
                                continue;
                        }
                        fprintf(stderr, "read() failed: %s\n", strerror(errno));
                        exit(1);
                }
                count += len;
	} while (count < packet_size);

	return 1;
}

/* Elapsed time between two struct timespec time stamps in milliseconds */
int tsdiff_ms(struct timespec *tstart, struct timespec *tend)
{
        return ( (tend->tv_sec - tstart->tv_sec)*1000 +
                        (tend->tv_nsec - tstart->tv_nsec)/1000000);
}

/* timeout in milliseconds */
int rx(int rx_fd, unsigned char *buffer, int packet_size, int timeout)
{
        struct pollfd pfd;
	ssize_t len = 0, count = 0;

        do {
                pfd.fd = rx_fd;
                pfd.events = POLLIN;
        
                switch(poll(&pfd, 1, timeout)) {
                case 0:
                        return 0;
                case -1:
                        fprintf(stderr, "poll() failed: %s\n", strerror(errno));
                        exit(1);
                default:
                        break;
                }
                len = read(rx_fd, &buffer[count], packet_size - count);
                if (len < 0) {
                        if (errno == EWOULDBLOCK) {
                                continue;
                        }
                        fprintf(stderr, "read() failed: %s\n", strerror(errno));
                        exit(1);
                }
                count += len; 
	} while (count < packet_size);

	return 1;
}

static int baud_to_constant_async(int speed)
{
#define B(x) case x: return B##x
        switch (speed) {
        B(1200); B(1800); B(2400); B(4800); B(9600);
        B(19200); B(38400); B(57600);
        B(115200); B(230400);
        default:
                fprintf(stderr, "invalid port speed %d, using 1200\n", speed);
                return B1200;
        }
}

void port_config_async(int fd, int speed)
{
        // set serial port to raw mode, set baudrate
        struct termios new_termios;
         
        if (tcgetattr(fd, &old_termios) < 0) {
                fprintf(stderr, "port_config error %s\n", strerror(errno));
                exit(1);
        }
    
        memcpy (&new_termios, &old_termios, sizeof(struct termios)); 
   	new_termios.c_cflag = B9600|CS8|CLOCAL|CREAD;
        new_termios.c_iflag = IGNBRK|IGNPAR;
        new_termios.c_oflag = 0;
        new_termios.c_lflag = 0;
        new_termios.c_cc[VTIME] = 0;
        new_termios.c_cc[VMIN] = 1;
        cfsetispeed(&new_termios,baud_to_constant_async(speed));
        cfsetospeed(&new_termios,baud_to_constant_async(speed));

        tcflush(fd,TCIFLUSH);
        if (tcsetattr(fd, TCSANOW, &new_termios) < 0) {
                fprintf(stderr, "port_config error %s\n", strerror(errno));
                exit(1);
        }
                
        /* Set flow control if required */
}

static int baud_to_constant_sync(int speed)
{
#define ATC_B(x) case x: return ATC_B##x
        switch (speed) {
        ATC_B(1200); ATC_B(2400); ATC_B(4800); ATC_B(9600);
        ATC_B(19200); ATC_B(38400); ATC_B(57600); ATC_B(76800);
        ATC_B(115200); ATC_B(153600); ATC_B(614400); 
        default:
                fprintf(stderr, "invalid port speed %d, using 153600\n", speed);
                return ATC_B153600;
        }
}

void port_config_sync(int fd, int speed)
{
        atc_spxs_config_t config = {ATC_SDLC, ATC_B614400, ATC_CLK_INTERNAL, ATC_GATED};
        
        config.baud = baud_to_constant_sync(speed);
        
        if(ioctl(fd, ATC_SPXS_WRITE_CONFIG, (unsigned long)&config) < 0) {
		fprintf(stderr, "ioctl ATC_SPXS_WRITE_CONFIG error %s\n",
                                strerror(errno));
                exit(1);
        }
}

const unsigned char test_packet[] = {
        0x3e, 0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6b,
        0xf7, 0xfe, 0x62, 0x72, 0x6f, 0x77, 0xbe, 0x6e, 0x5f, 0x66,
        0x6f, 0x78, 0x20, 0x6a, 0x75, 0x6d, 0x70, 0x73, 0x20, 0x7d,
        0x6f, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x7e, 0x7c,
        0x61, 0x7a, 0x79, 0x20, 0x64, 0x6f, 0x67, 0x3f, 0xfe};

void ser_test(char *port1, char *port2, int port_speed, int number_of_packets, int packet_size)
{
        unsigned char *buffer;
	int tx_fd, rx_fd, tx_cnt = 0, rx_cnt = 0;
	struct timespec ts;
	struct timeval tx_first, rx_last, current;
        int i = 0, j = 0;

        if ((tx_fd = open(port1, O_RDWR|O_NONBLOCK)) < 0) {
                fprintf(stderr, "Could not open serial port %s error %s\n",
                        port1, strerror(errno));
                exit(1);
        }
        if (port1[strlen(port1)-1] == 's')
                port_config_sync(tx_fd, port_speed);
        else
                port_config_async(tx_fd, port_speed);


        if (strcmp(port1, port2) != 0) {
                if ((rx_fd = open(port2, O_RDWR|O_NONBLOCK)) < 0) {
                        fprintf(stderr, "Could not open serial port %s error %s\n",
                                port2, strerror(errno));
                        exit(1);
                }
                if (port2[strlen(port2)-1] == 's')
                        port_config_sync(rx_fd, port_speed);
                else
                        port_config_async(rx_fd, port_speed);
        } else {
                rx_fd = tx_fd;
        }

	if (!(buffer = calloc(packet_size * 2, 1))) {
		fprintf(stderr, "Out of memory\n");
                exit(1);
        }

        i=0;
        while (i<packet_size) {
		for (j=0;(i<packet_size)&&(j<(int)sizeof(test_packet)); i++,j++) {
			buffer[i] = test_packet[j];
		}
        }
        
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;

	if (gettimeofday(&tx_first, NULL)) {
		fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
                exit(1);
        }
	rx_last = tx_first;

	while (tx_cnt < number_of_packets || rx_cnt < number_of_packets) {
		int t = 0, r = 0;

		if (tx_cnt < number_of_packets) {
                        /* timeout in milliseconds based on slowest baud rate (1200) */
			t = tx(tx_fd, buffer, packet_size, (packet_size*2)*10000/1200);
                        if (!t) {
                                //printf("tx:could not write, txcnt=%d, rxcnt=%d\n", tx_cnt, rx_cnt);
                                nanosleep(&ts, NULL);
                        }
                }
                /* timeout in milliseconds based on slowest baud rate (1200) */
		r = rx(rx_fd, buffer + packet_size, packet_size, (packet_size*2)*10000/1200);
		if (r) {
			if (gettimeofday(&rx_last, NULL)) {
				fprintf(stderr, "gettimeofday() failed: %s\n",
				      strerror(errno));
                                      exit(1);
                        }
                        if (memcmp(buffer, buffer+packet_size, packet_size) != 0) {
                                fprintf(stderr, "rx packet #%d differs from tx packet %d: %2x %2x %2x %2x %2x\n",
                                        rx_cnt, tx_cnt,
                                        buffer[packet_size], buffer[packet_size+1],
                                        buffer[packet_size+2], buffer[packet_size+3],
                                        buffer[packet_size+4]);
                                exit(1);
                        }
                } else {
                        //printf("rx: no data read, txcnt=%d, rxcnt=%d\n", tx_cnt+t, rx_cnt);
                        nanosleep(&ts, NULL);
                }
		tx_cnt += t;
		rx_cnt += r;

		if (!t && !r) {
			if (tx_cnt == number_of_packets) {
				if (gettimeofday(&current, NULL)) {
					fprintf(stderr, "gettimeofday() failed: %s\n",
					      strerror(errno));
                                        exit(1);
                                }
				if (current.tv_sec - rx_last.tv_sec +
				    (current.tv_usec - rx_last.tv_usec) /
				    1000000 > 10) /* timeout 10 s */
					break;
			}
			nanosleep(&ts, NULL);
		}
	}

        if (port1[strlen(port1)-1] != 's')
                tcsetattr(tx_fd, TCSANOW, &old_termios);
        if (strcmp(port1, port2) != 0) {
                if (port2[strlen(port2)-1] != 's')
                        tcsetattr(rx_fd, TCSANOW, &old_termios);
        }
	close(tx_fd);
	close(rx_fd);
        free(buffer);
	printf("%u packet%s sent to %s\n%u packet%s received from %s\n",
	       tx_cnt, tx_cnt != 1 ? "s" : "", port1,
	       rx_cnt, tx_cnt != 1 ? "s" : "", port2 );
	if (rx_cnt)
		printf("approximate transfer speed: %.3f kbps\n",
		       packet_size * 10 * rx_cnt /
		       ((rx_last.tv_sec - tx_first.tv_sec) * 1000.0 +
			(rx_last.tv_usec - tx_first.tv_usec) / 1000.0));
        if (rx_cnt != tx_cnt) {
                printf("packet loss occurred\n");
                exit(1);
        }
}

int main(int argc, char *argv[])
{
        int port_speed = 1200;
	int number_of_packets = 1000;
	int packet_size = 1024;
        char *port1, *port2, dummy;
        
	if (argc < 2 || argc > 5)
		usage();

        if (argc >= 3)
		if (sscanf(argv[2], "%u%c", &port_speed, &dummy) != 1)
			usage();
	if (argc >= 4)
		if (sscanf(argv[3], "%u%c", &number_of_packets, &dummy) != 1)
			usage();
	if (argc >= 5)
		if (sscanf(argv[4], "%u%c", &packet_size, &dummy) != 1)
			usage();

	port1 = argv[1];
	if ((port2 = strchr(argv[1], ':')))
		*(port2++) = '\x0';

	if (port1[0] == '\x0') {
		fprintf(stderr, "Empty serial port name\n");
                exit(1);
        }

	if (port2)
		if (port2[0] == '\x0') {
			fprintf(stderr, "Empty serial port name\n");
                        exit(1);
                }
        
	ser_test(port1, port2 ? port2 : port1, port_speed,
		  number_of_packets, packet_size);

	exit(0);
}
