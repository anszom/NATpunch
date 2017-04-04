#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

int main(int ac, char ** av)
{
	struct sockaddr_in local1, local2, remote1, remote2;
	int sock1, sock2;
	time_t t1=0, t2=0;
	local1.sin_family = AF_INET;
	local1.sin_addr.s_addr = 0;
	local1.sin_port = htons(atoi(av[1]));
	local2.sin_family = AF_INET;
	local2.sin_addr.s_addr = 0;
	local2.sin_port = htons(atoi(av[2]));

	memset(&remote1, 0, sizeof(remote1));
	memset(&remote2, 0, sizeof(remote2));
	
	sock1 = socket(AF_INET, SOCK_DGRAM, 0);	
	sock2 = socket(AF_INET, SOCK_DGRAM, 0);
	bind(sock1, (struct sockaddr*)&local1, sizeof(local1));
	bind(sock2, (struct sockaddr*)&local2, sizeof(local2));

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
		select(sock2+1, &fds, NULL, NULL, NULL);
		
		time(&now);

		rv = recvfrom(sock1, rcvbuf, sizeof(rcvbuf), MSG_DONTWAIT, (struct sockaddr*)&pkt_addr, &pkt_alen);
		if(rv > 0) {
			remote1 = pkt_addr;
			t1 = now;
			printf("ping1\n");
			if(remote2.sin_port > 0 && now-t2 < 5) 
				sendto(sock1, &remote2, sizeof(remote2), 0, (struct sockaddr*)&pkt_addr, pkt_alen);
		}

		rv = recvfrom(sock2, rcvbuf, sizeof(rcvbuf), MSG_DONTWAIT, (struct sockaddr*)&pkt_addr, &pkt_alen);
		if(rv > 0) {
			remote2 = pkt_addr;
			t2 = now;
			printf("ping2\n");
			if(remote1.sin_port > 0 && now-t1 < 5) 
				sendto(sock2, &remote1, sizeof(remote1), 0, (struct sockaddr*)&pkt_addr, pkt_alen);
		}
	}
	return 0;
}
