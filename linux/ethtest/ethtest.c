/*
 * ethtest.c
 *
 * Copyright (C) 2017 Intelight Inc. <mike.gallagher@intelight-its.com>
 *
 * A low-level Ethernet external loopback test.
 * (Based on hdlctest.c Copyright (C) 2006 Krzysztof Halasa)
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
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/if_ether.h>
#include <linux/if.h>
#include <linux/sockios.h>

int if_sock = -1;
struct ifreq *ifr_tab[2] = { NULL, NULL };
unsigned char *test_buffer = NULL;

static void error(const char *format, ...) __attribute__ ((__noreturn__));
static void usage(void) __attribute__ ((__noreturn__));
static void ifconfig(struct ifreq *ifr, int up);

static void error(const char *format, ...)
{
	va_list args;
	struct ifreq *ptr;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);


	if ((ptr = ifr_tab[0])) {
		ifr_tab[0] = NULL;
		ifconfig(ptr, 0);
	}

	if ((ptr = ifr_tab[1])) {
		ifr_tab[1] = NULL;
		ifconfig(ptr, 0);
	}

        free (test_buffer);
	exit(1);
}


static void usage(void)
{
	fprintf(stderr, "ethtest version 1.0\n"
		"\n"
		"Usage: ethtest (ethX | ethX:ethY)"
		" [number_of_packets [packet_size]]\n");
	exit(1);
}


static int tx(int sock, struct sockaddr_ll *addr, uint8_t *buffer,
       unsigned packet_size)
{
	if (sendto(sock, buffer, packet_size, 0,
		   (struct sockaddr*)addr, sizeof(struct sockaddr_ll)) < 0) {
		if (errno == ENOBUFS)
			return 0;
		error("sendto() failed: %s\n", strerror(errno));
	}
	return 1;
}


static int rx(int sock, struct sockaddr_ll *addr, uint8_t *buffer,
	      unsigned int packet_size)
{
	ssize_t len;

	do {
		socklen_t from_len = sizeof(struct sockaddr_ll);
		len = recvfrom(sock, buffer, packet_size, MSG_DONTWAIT,
			       (struct sockaddr*)addr, &from_len);

		if (len < 0) {
			if (errno == EAGAIN)
				return 0;
			error("recvfrom() failed: %s\n", strerror(errno));
		}
	} while (addr->sll_pkttype == PACKET_OUTGOING);

//	if (len != (ssize_t)packet_size) {
//		fprintf(stderr, "Warning: received packet %i bytes, expected %i\n",
//			len, packet_size);
//		return 0;
//	}

	return len;
}

const unsigned char bcast[] = {0xff,0xff,0xff,0xff,0xff,0xff};
const unsigned char test_packet[] = {
        0x3e, 0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6b,
        0xf7, 0xfe, 0x62, 0x72, 0x6f, 0x77, 0xbe, 0x6e, 0x5f, 0x66,
        0x6f, 0x78, 0x20, 0x6a, 0x75, 0x6d, 0x70, 0x73, 0x20, 0x7d,
        0x6f, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x7e, 0x7c,
        0x61, 0x7a, 0x79, 0x20, 0x64, 0x6f, 0x67, 0x3f, 0xfe};

static void eth_test(struct ifreq *tx_ifr, struct ifreq *rx_ifr,
	       unsigned number_of_packets, unsigned packet_size1,
	       unsigned packet_size2)
{
        struct sockaddr_ll tx_addr, rx_addr;
        unsigned char *tx_buffer, *rx_buffer;
	unsigned int tx_cnt = 0, rx_cnt = 0;
	int tx_sock, rx_sock;
	struct timespec ts;
	struct timeval tx_first, rx_last, current;
        unsigned int i = 0, j = 0, packet_size;

	packet_size = packet_size1;
	if (packet_size2 > packet_size)
		packet_size = packet_size2;
		
	if (!(tx_buffer = calloc(packet_size, 1)) || !(rx_buffer = calloc(packet_size, 1)))
		error("Out of memory\n");

        i=0;
        while (i<packet_size) {
		for (j=0;(i<packet_size)&&(j<sizeof(test_packet)); i++,j++) {
			tx_buffer[i] = test_packet[j];
		}
        }
        
	if (ioctl(if_sock, SIOCGIFINDEX, tx_ifr))
		error("Unable to get %s device index: %s\n", tx_ifr->ifr_name,
		      strerror(errno));

	if (ioctl(if_sock, SIOCGIFINDEX, rx_ifr))
		error("Unable to get %s device index: %s\n", rx_ifr->ifr_name,
		      strerror(errno));

	memset(&tx_addr, 0, sizeof(tx_addr));
	tx_addr.sll_family = AF_PACKET;
	tx_addr.sll_protocol = htons(ETH_P_802_2);
	tx_addr.sll_ifindex = tx_ifr->ifr_ifindex;
        tx_addr.sll_halen = ETH_ALEN;
        memcpy(tx_addr.sll_addr, bcast, ETH_ALEN);

	memset(&rx_addr, 0, sizeof(rx_addr));
	rx_addr.sll_family = AF_PACKET;
	rx_addr.sll_protocol = htons(ETH_P_ALL);
	rx_addr.sll_ifindex = rx_ifr->ifr_ifindex;

	if ((tx_sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_802_2))) < 0 ||
	    (rx_sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_ALL))) < 0)
                error("socket() failed\n");
	if (bind(rx_sock, (struct sockaddr*)&rx_addr, sizeof(rx_addr)) < 0)
		error("bind() failed\n");
	ts.tv_sec = 0;
	ts.tv_nsec = 1000000;

	if (gettimeofday(&tx_first, NULL))
		error("gettimeofday() failed: %s\n", strerror(errno));
	rx_last = tx_first;

	packet_size = packet_size1;
	while (tx_cnt < number_of_packets || rx_cnt < number_of_packets) {
		int t = 0, r = 0;

		if (tx_cnt < number_of_packets)
			t = tx(tx_sock, &tx_addr, tx_buffer, packet_size);

		r = rx(rx_sock, &rx_addr, rx_buffer, packet_size);
		if (r) {
			if (gettimeofday(&rx_last, NULL))
				error("gettimeofday() failed: %s\n",
				      strerror(errno));
                        if (memcmp(tx_buffer, rx_buffer, r) != 0)
                                error("rx packet %d differs from tx packet\n", tx_cnt);
                        r = 1;
                }
		tx_cnt += t;
		rx_cnt += r;

		if (!t && !r) {
			if (tx_cnt == number_of_packets) {
				if (gettimeofday(&current, NULL))
					error("gettimeofday() failed: %s\n",
					      strerror(errno));
				if (current.tv_sec - rx_last.tv_sec +
				    (current.tv_usec - rx_last.tv_usec) /
				    1000000 > 2) /* timeout 2 s */
					break;
			}
			nanosleep(&ts, NULL);
		}
		packet_size = (packet_size == packet_size1)?packet_size2:packet_size1;
	}

	close(tx_sock);
	close(rx_sock);
        free(tx_buffer);
        free(rx_buffer);
	printf("%u packet%s sent to %s\n%u packet%s received from %s\n",
	       tx_cnt, tx_cnt != 1 ? "s" : "", tx_ifr->ifr_name,
	       rx_cnt, tx_cnt != 1 ? "s" : "", rx_ifr->ifr_name);
	packet_size = (packet_size1 + packet_size2)/2;
	if (rx_cnt)
		printf("approximate transfer speed: %.3f kbps\n",
		       (packet_size + sizeof(struct ethhdr)) * 10 * rx_cnt /
		       ((rx_last.tv_sec - tx_first.tv_sec) * 1000.0 +
			(rx_last.tv_usec - tx_first.tv_usec) / 1000.0));
        if (rx_cnt != tx_cnt)
                error("packet loss occurred\n");
}


static void ifconfig(struct ifreq *ifr, int up)
{

	if (!ifr)
		return;

	if (ioctl(if_sock, SIOCGIFFLAGS, ifr))
		error("Unable to get %s device flags: %s\n", ifr->ifr_name,
		      strerror(errno));
	if (up)
		ifr->ifr_flags |= (IFF_PROMISC);
	else
		ifr->ifr_flags &= ~(IFF_PROMISC);

	if (ioctl(if_sock, SIOCSIFFLAGS, ifr))
		error("Unable to set device flags: %s\n", ifr->ifr_name,
		      strerror(errno));

}


int main(int argc, char *argv[])
{
	unsigned int number_of_packets = 1000;
	unsigned int packet_size1 = 1024;
	unsigned int packet_size2 = 512;
	char *if1, *if2, dummy;
	struct timespec ts;
	struct ifreq ifr[2], *rx_ifr = NULL;

	if (argc < 2 || argc > 4)
		usage();

	if (argc >= 3)
		if (sscanf(argv[2], "%u%c", &number_of_packets, &dummy) != 1)
			usage();
	if (argc >= 4) {
		if (sscanf(argv[3], "%u%c", &packet_size1, &dummy) != 1)
			usage();
		packet_size2 = packet_size1;
	}
	if1 = argv[1];
	if ((if2 = strchr(argv[1], ':')))
		*(if2++) = '\x0';

	if (if1[0] == '\x0')
		error("Empty interface name\n");
	if (strlen(if1) > IFNAMSIZ - 1)
		error("Interface name %s too long\n", if1);
	strcpy(ifr[0].ifr_name, if1);

	if (if2) {
		if (if2[0] == '\x0')
			error("Empty interface name\n");
		if (strlen(if2) > IFNAMSIZ - 1)
			error("Interface name %s too long\n", if2);
		strcpy(ifr[1].ifr_name, if2);
		rx_ifr = &ifr[1];
	}

	if ((if_sock = socket(PF_PACKET, SOCK_DGRAM, IPPROTO_IP)) < 0)
		error("Error creating control socket: %s\n", strerror(errno));

	ts.tv_sec = 0;
	ts.tv_nsec = 100000;

	ifconfig(&ifr[0], 1);
	ifr_tab[0] = &ifr[0];
	ifconfig(rx_ifr, 1);
	ifr_tab[1] = rx_ifr;

	nanosleep(&ts, NULL);

	eth_test(&ifr[0], rx_ifr ? rx_ifr : &ifr[0],
		  number_of_packets, packet_size1, packet_size2);

	ifconfig(&ifr[0], 0);	ifconfig(rx_ifr, 0);
	nanosleep(&ts, NULL);

	exit(0);
}
