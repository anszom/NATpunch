#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>

#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include "common.h"

int tun_alloc(int fd, const char *name, struct sockaddr_in *tun_local, struct sockaddr_in *tun_remote, int tun_mtu)
{
	struct ifreq ifr;
	int err;
	int tun_fd = open("/dev/net/tun", O_RDWR);
	if(tun_fd < 0) {
		perror("open /dev/net/tun");
		return tun_fd;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI; 
	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	if((err = ioctl(tun_fd, TUNSETIFF, &ifr)) < 0) {
		close(tun_fd);
		return err;
	}

	memcpy(&ifr.ifr_addr, tun_local, sizeof(struct sockaddr));
	if((err = ioctl(fd, SIOCSIFADDR, &ifr)) < 0)
		perror("SIOCSIFADDR");
	
	memcpy(&ifr.ifr_dstaddr, tun_remote, sizeof(struct sockaddr));
	if((err = ioctl(fd, SIOCSIFDSTADDR, &ifr)) < 0)
		perror("SIOCSIFDSTADDR");

	ifr.ifr_mtu = tun_mtu;
	if((err = ioctl(fd, SIOCSIFMTU, &ifr)) < 0)
		perror("SIOCSIFMTU");

	if((err = ioctl(fd, SIOCGIFFLAGS, &ifr)) < 0)
		perror("SIOCGIFFLAGS");
	else {
		ifr.ifr_flags = IFF_UP | IFF_RUNNING;
		if((err = ioctl(fd, SIOCSIFFLAGS, &ifr)) < 0)
			perror("SIOCSIFFLAGS");
	}

	return tun_fd;
}              

int addrcmp(struct sockaddr_in *a, struct sockaddr_in *b)
{
	return !(a->sin_port == b->sin_port && a->sin_addr.s_addr == b->sin_addr.s_addr);
}

int main(int ac, char ** av)
{
	struct sockaddr_in sin_server;
	struct hostent *hent;
	struct sockaddr_in tun_local, tun_remote;
	int tun_mtu;
	int sock, tun=-1;

	if(ac != 7) {
		fprintf(stderr, "NATpunch client by Andrzej Szombierski <qq@kuku.eu.org>\n"
				"Run this code on both computers running behind different NATs.\n"
				"This program may require root privileges for opening a linux tun interface\n"
				"\n"
				"Usage: %s handshake-server handshake-port tun-name tun-local tun-remote tun-mtu\n"
				"\n"
				"Example:\n"
				"\tserver: server 2000 2001\n"
				"\tpeer 1: %s <server name> 2000 tun0 10.10.0.1 10.10.0.2 1000\n"
				"\tpeer 2: %s <server name> 2001 tun0 10.10.0.1 10.10.0.2 1000\n"
				"\n"
				"Warning: the tunnel is NOT SECURED in any way. See README!\n",
				av[0], av[0], av[0]);
		return 1;
	}

	hent = gethostbyname(av[1]);
	if(!hent) {
		herror("gethostbyname");
		return 1;
	}

	sin_server.sin_family = AF_INET;
	sin_server.sin_port = htons(atoi(av[2]));
	memcpy(&sin_server.sin_addr, hent->h_addr_list[0], 4);

	tun_local.sin_family = AF_INET;
	tun_remote.sin_family = AF_INET;
	inet_aton(av[4], &tun_local.sin_addr);
	inet_aton(av[5], &tun_remote.sin_addr);
	tun_mtu = atoi(av[6]);
	
	// be persistent
	for(;;) {
		struct sockaddr_in sin_local;
		int sock = socket(AF_INET, SOCK_DGRAM, 0);	

		if(sock < 0) {
			perror("socket");
			return 1;
		}

		sin_local.sin_family = AF_INET;
		sin_local.sin_port = 0;
		sin_local.sin_addr.s_addr = 0;

		// bind a new port on each attempt, hopefully avoiding any problems with stale rules on a NAT router
		if(bind(sock, (struct sockaddr*)&sin_local, sizeof(sin_local))) {
			perror("bind");
			return 1;
		}

		struct sockaddr_in sin_remote;
		time_t last_sent=0, last_rcvd=0;

		printf("Pinging the handshake server %s:%s\n", av[1], av[2]);
		memset(&sin_remote, 0, sizeof(sin_remote));
		while(sin_remote.sin_port == 0) {
			struct sockaddr_in pkt_addr;
			struct portable_sockaddr4 rcvbuf;
			int rv;
			socklen_t pkt_alen = sizeof(pkt_addr);

			sendto(sock, "A", 1, 0, (struct sockaddr*)&sin_server, sizeof(sin_server));
			while((rv=recvfrom(sock, &rcvbuf, sizeof(rcvbuf), MSG_DONTWAIT, (struct sockaddr*)&pkt_addr, &pkt_alen)) > 0) {
				if(rv == sizeof(rcvbuf) && !addrcmp(&pkt_addr, &sin_server)) {
					sin_remote.sin_family = AF_INET;
					sin_remote.sin_port = rcvbuf.port;
					sin_remote.sin_addr.s_addr = rcvbuf.host;
					break;
				}
			}
			sleep(1);
		}
				
		printf("Tunneling to addr: %s:%d\n", inet_ntoa(sin_remote.sin_addr), ntohs(sin_remote.sin_port));

		if(tun < 0) {
			tun = tun_alloc(sock, av[3], &tun_local, &tun_remote, tun_mtu);
			if(tun < 0) {
				fprintf(stderr, "Tunnel interface setup failed, exiting\n");
				return 2;
			}
		}

		fcntl(tun, F_SETFL, O_NONBLOCK);
			
		time(&last_rcvd);

		// tunneling
		for(;;) {
			struct sockaddr_in pkt_addr;
			socklen_t pkt_alen = sizeof(pkt_addr);
			char buf[4096];
			fd_set fds;
			int rv;
			struct timeval tv;
			time_t now;
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			FD_ZERO(&fds);
			FD_SET(tun, &fds);
			FD_SET(sock, &fds);
			select(tun+1, &fds, NULL, NULL, &tv);

			time(&now);

			rv = recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT, (struct sockaddr*)&pkt_addr, &pkt_alen);
			if(rv > 0) {
				//printf("Got packet from %s:%d\n", inet_ntoa(pkt_addr.sin_addr), ntohs(pkt_addr.sin_port));
				if(!addrcmp(&pkt_addr, &sin_remote)) {
					if(rv > 1) // we use 1-byte packets for keepalives
						write(tun, buf, rv);
					last_rcvd = now;
				}
			}

			rv = read(tun, buf, sizeof(buf));
			if(rv > 0) {
				if(sendto(sock, buf, rv, MSG_DONTWAIT, (struct sockaddr*)&sin_remote, sizeof(sin_remote)) == rv)
					last_sent = now;
			}

			if(now - last_sent > 5) {
				// send keepalive packets
				if(sendto(sock, "\x00", 1, MSG_DONTWAIT, (struct sockaddr*)&sin_remote, sizeof(sin_remote)) == 1)
					last_sent = now;
			}

			if(now - last_rcvd > 15) {
				// didn't receive keepalive packets
				printf("Didn't receive keepalive packets, attempting to restart tunnel\n");
				break;
			}
		}

		close(sock);
	}

	return 0;
}
