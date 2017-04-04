// Copyright 2016 Andrzej Szombierski
// License: GPLv2, see LICENSE for full text
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "common.h"

int listen_on_port(int port)
{
	struct sockaddr_in local;
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if(sock < 0) {
		perror("socket");
		return sock;
	}

	local.sin_family = AF_INET;
	local.sin_addr.s_addr = 0;
	local.sin_port = htons(port);
	
	if(bind(sock, (struct sockaddr*)&local, sizeof(local))) {
		perror("bind");
		close(sock);
		return -1;
	}

	return sock;
}
int main(int ac, char ** av)
{
	struct portable_sockaddr4 remote1, remote2;
	int sock1, sock2;
	time_t t1=0, t2=0;

	if(ac != 3) {
		fprintf(stderr, "NATpunch handshake server by Andrzej Szombierski <qq@kuku.eu.org>\n"
				"Run this code on a computer with a public IP address\n"
				"\n"
				"Usage: %s port1 port2\n", av[0]);
		return 1;
	}

	sock1 = listen_on_port(atoi(av[1]));
	if(sock1 < 0)
		return 1;
	sock2 = listen_on_port(atoi(av[2]));
	if(sock2 < 0)
		return 1;

	memset(&remote1, 0, sizeof(remote1)); // peer addresses are unknown
	memset(&remote2, 0, sizeof(remote2));

	fprintf(stderr, "NATpunch server ready.\n");
	
	for(;;) {
		struct sockaddr_in pkt_addr;
		socklen_t pkt_alen = sizeof(pkt_addr);
		int rv;
		fd_set fds;
		char rcvbuf[4];
		time_t now;

		FD_ZERO(&fds);
		FD_SET(sock1, &fds);
		FD_SET(sock2, &fds);
		select((sock1<sock2?sock2:sock1)+1, &fds, NULL, NULL, NULL);
		
		time(&now);

		// receive packets from peer 1
		rv = recvfrom(sock1, rcvbuf, sizeof(rcvbuf), MSG_DONTWAIT, (struct sockaddr*)&pkt_addr, &pkt_alen);
		if(rv > 0) {
			// record peer 1's address
			remote1.host = pkt_addr.sin_addr.s_addr;
			remote1.port = pkt_addr.sin_port;
			t1 = now;
			printf("peer 1 called\n");
			if(remote2.port > 0 && now-t2 < 5)  // check if peer 2's address is known and recent
				// forward peer 2's address to peer 1
				sendto(sock1, &remote2, sizeof(remote2), 0, (struct sockaddr*)&pkt_addr, pkt_alen);
		}

		// the same for peer 2
		rv = recvfrom(sock2, rcvbuf, sizeof(rcvbuf), MSG_DONTWAIT, (struct sockaddr*)&pkt_addr, &pkt_alen);
		if(rv > 0) {
			remote2.host = pkt_addr.sin_addr.s_addr;
			remote2.port = pkt_addr.sin_port;
			t2 = now;
			printf("peer 2 called\n");
			if(remote1.port > 0 && now-t1 < 5) 
				sendto(sock2, &remote1, sizeof(remote1), 0, (struct sockaddr*)&pkt_addr, pkt_alen);
		}
	}
	return 0;
}
