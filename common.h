#include <inttypes.h>

// sockaddr_in will likely be the same on both sides, but, to be safe, 
// we send this struct instead of sockaddr_in
struct portable_sockaddr4 {
	uint32_t host;
	uint32_t port;
};
