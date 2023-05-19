// Netwrite *client* in C
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <termios.h>
#include <fcntl.h>


static struct termios oldt;
void setNoncanonical(){
	struct termios new;
	tcgetattr(STDIN_FILENO, &oldt);
	new = oldt;
	new.c_lflag &= ~(ICANON);
	tcsetattr( STDIN_FILENO, TCSANOW, &new);
}

void exitNoncanonical(){
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
}


int main(int argc, char** argv){
	char name[100] = { 0 };
	char domain[100] = { 0 };
	int nameSize = 0;
	int domainSize = 0;
	int phase = 0;
	printf("%s, %s\n", argv[0], argv[1]);
	for (int i = 0; i < strlen(argv[1]); i ++){
		char x = argv[1][i];
		if (phase == 0){
			if (x == '@'){
				phase = 1;
			}
			else{
				name[nameSize] = x;
				nameSize ++;
			}
		}
		else if (phase == 1){
			domain[domainSize] = x;
			domainSize ++;
		}
	}
	struct addrinfo* info;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV | AI_CANONNAME;
	if (getaddrinfo(domain, "6780", &hints, &info) != 0){
		perror("getaddrinfo failed");
		return 1;
	}
	int sock = 0;
	struct addrinfo* rp;
	char ipstr [INET_ADDRSTRLEN];
	if (info == NULL){
		printf("No address found\n");
		return 1;
	}
	for (rp = info; rp != NULL; rp = rp -> ai_next){ // THANKS, man getaddrinfo(3)
		struct in_addr  *addr;  
	        struct sockaddr_in *ipv = (struct sockaddr_in *)rp->ai_addr; 
        	addr = &(ipv->sin_addr);
        	inet_ntop(rp->ai_family, addr, ipstr, sizeof ipstr); 
		printf("Got address %s\n", ipstr);
		sock = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
		if (sock == -1){
			printf("Failed to create socket\n");
			continue;
		}
		//(struct sockaddr_in*)(rp -> ai_addr -> s_addr).sin_port = htons(6780);
		if (connect(sock, rp -> ai_addr, rp -> ai_addrlen) == 0){
			break; // success
		}
		close(sock);
	}
	freeaddrinfo(info);
	if (sock == 0){ // No addresses succeeded - further THANKS, man getaddrinfo(3)
		printf("No address connected\n");
		exit(1);
	}
	char* uName = getlogin();
	send(sock, uName, strlen(uName), 0);
	send(sock, " ", 1, 0);
	send(sock, name, strlen(name), 0);
	send(sock, "\n", 1, 0);
	setNoncanonical();
	fcntl(0, F_SETFL, fcntl(0, F_GETFL, NULL) | O_NONBLOCK); // Non blocking STDIN
	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, NULL) | O_NONBLOCK); // Non blocking socket
	char c;
	while (1){
		c = getchar();
		if (c == 127){
			printf("\b\b\b   \b\b\b");
			send(sock, "\b \b", 3, 0);
		}
		else{
			send(sock, &c, 1, 0);
		}
	}
	exitNoncanonical();
	return 0;
}
