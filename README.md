NATpunch
--------

NATpunch is a simple program designed to allow direct communication between
two linux machines running behind different NAT's. A server with a public
IP is required for the initial handshake, afterwards, data is sent directly
between the communicating peers.

NATpunch configures a linux "tun" interface on the peers, and forwards the
IP packets inside UDP packets over the internet. NAT traversal is performed
using a technique called "UDP hole punching" (see wikipedia).

NATpunch tries to work with NAT routers which alter UDP port numbers, but
it may not work for especially stubborn (evil?) implementations. With my 
ISP, it works where others (chownat/pwnat) fail. That's good enough for me :)

NATpunch requires root privileges (or at least cap_net_admin) to operate
the tun interface. Please be aware that any bugs in this program may 
severely impact your security.

Also, the program was hacked together in about two hours, so don't expect too
much.

Usage
-----
Let's say that *peer1* and *peer2* are behind NAT, while *server* has a public
IP. On *server*, run:

	./server 2000 2001

The server will run forever until stopped. You can kill it after the initial
handshake, but it's better to leave it running in case the connection drops.
UDP ports 2000 and 2001 will be used by peers 1 and 2 to exchange "handshake"
information. Obviously the firewall needs to allow these packets to pass.

On *peer1*, run (with root privileges):

	./client server.address.com 2000 tun0 10.10.0.1 10.10.0.2 1000

On *peer2*, run (with root privileges):

	./client server.address.com 2001 tun0 10.10.0.2 10.10.0.1 1000
	(note the port change and address swap)

If all goes well, after several seconds, a tunnel will be ready. On the tunnel
interface, *peer1* have the address 10.10.0.1, while *peer2* will be 10.10.0.2.
The client process will run until killed. When killed, the tunnel interface
will be automatically removed by the kernel.
The client process will send a "keepalive" packet if the link is idle for 5
seconds. If no packets are received for 15 seconds, the tunnel is assumed to
be dead, and the handshake procedure is retried (forever, if necessary). 

Try on *peer1*:

	ping 10.10.0.2


Try on *peer2*:

	ping 10.10.0.1

Setting up more interesting routing is left as an excercise for the reader :)

Security
--------
TL,DR: **using NATpunch may reduce your security**

The handshake sequence is not authenticated in any way. Anyone sending data
to the handshake ports may disrupt the handshake process.

The tunnel is not encrypted or protected in any way. The data passed through
the tunnel is sent as plaintext over the Internet. **Please use only secure
protocols (SSH, TLS, ...), just as you would over the Internet**. 

What is worse, anyone who guesses/sniffs your peers' IP/ports, can send spoofed
packets into your tunnel. These will happily pass through your firewall
(otherwise the tunnel wouldn't actually work:) and come out of tun0. **Please set
up proper iptables rules for your tunnel interface!**

