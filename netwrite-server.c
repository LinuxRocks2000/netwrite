/* write (see man write(1)), but over networks. This is a server implementation using the nonblocking socket APIs and poll(2); see netwrite.c for a compliant client. */
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdio.h>
#include <fcntl.h>
#include <stdbool.h>
#include <utmp.h>

#define PORT 6780

struct client{
	int fd;
	FILE* target; // tty it's writing to; keep it 0 until you get the tty path somehow
	char nameBuf[100]; // "name" of the client - probably in username@domain syntax.
	char targetUsernameBuf[100]; // target username; @domain not included.
	int procPhase; // 0 = loading client name, 1 = loading target username, 2 = normal operation receiving messages
	struct client* next; // Linked list - easier, perhaps, than regular allocation, but far less efficient
};


int server;
struct sockaddr_in servaddr;
char buf[1024] = { 0 };

int tru = true; // look, don't ask, it's a crappy piece of code enforced by the syscall stuff.

struct client* first = 0;

struct client* newClient(int fd){
	struct client* ret = malloc(sizeof(struct client));
	memset(ret, 0, sizeof(struct client)); // 0 it
	ret -> fd = fd;
	return ret;
}

struct client* appendClient(int fd){
	struct client* n = newClient(fd);
	if (first == 0){
		first = n;
	}
	else{
		struct client* c = first;
		while (c -> next){
			c = c -> next;
		}
		c -> next = n;
	}
	return n;
}


char* strMerge(char* one, char* two){
	char* ret = malloc(strlen(one) + strlen(two) + 1);
	ret[strlen(one) + strlen(two)] = 0;
	for (int x = 0; x < strlen(one); x ++){
		ret[x] = one[x];
	}
	for (int x = 0; x < strlen(two); x ++){
		ret[x + strlen(one)] = two[x];
	}
	return ret;
}


char* chooseTTY(char* name){
	struct utmp* u;
	time_t bestTime = 0;
	setutent();
	char* winner = malloc(1); // make it a valid string, but only a tiny one, so it can be safely freed. Means slightly less logic in places.
	while (u = getutent()){
		if (strcmp(u -> ut_user, name) == 0){
			if (u -> ut_tv.tv_sec > bestTime){
				bestTime = u -> ut_tv.tv_sec;
				free(winner);
				winner = strMerge("/dev/", u -> ut_line);
			}
		}
	}
	if (bestTime == 0){
		free(winner);
		return 0;
	}
	return winner;
}


bool doClientWork(struct client* c){
	char chr;
	while (true){
		int ret = recv(c -> fd, &chr, sizeof(char), 0);
		if (ret == 0){ // Read One Char.
			// As it turns out, 0 is a reserved return value. It *always* means the socket is closed.
			return true;
		}
		if (ret == -1){
			break; // Out of data - or an error occurred. Either way exit. Check errors later maybe?
		}
		if (c -> procPhase == 0){
			if (chr == ' '){
				c -> procPhase = 1;
			}
			else{
				c -> nameBuf[strlen(c -> nameBuf)] = chr; // SEGFAULT WARNING: THIS PERMITS NAMES LONGER THAN 99 CHARACTERS WHICH WILL TRIP A SEGMENTATION FAULT BECAUSE OF STRLEN.
			}
		}
		else if (c -> procPhase == 1){
			if (chr == '\n'){
				printf("Established target: %s\n", c -> targetUsernameBuf);
				c -> procPhase = 2;
				char* filePath = chooseTTY(c -> targetUsernameBuf);
				if (filePath){
					c -> target = fopen(filePath, "w");
					fprintf(c -> target, "\n---- Incoming message from %s ----\n", c -> nameBuf);
				}
				else{
					c -> procPhase = -1; // Lock the client - it didn't work, waouw. Add a descriptive error broadcast here later.
				}
			}
			else if (chr != '\r'){
				c -> targetUsernameBuf[strlen(c -> targetUsernameBuf)] = chr;
			}
		}
		else if (c -> procPhase == 2){
			fputc(chr, c -> target);
			fflush(c -> target);
		}
	}
	return false;
}


int main(){
	server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0){
		perror("Socket Creation Failed");
		exit(1);
	}
	if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &tru, sizeof(tru))){
		perror("Setsockopt failed");
		exit(1);
	}
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = INADDR_ANY;
	servaddr.sin_port = htons(PORT);
	if (bind(server, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		perror("Bind failed");
		exit(1);
	}
	if (listen(server, 25) < 0){
		perror("Listen failed");
		exit(1);
	}
	if (fcntl(server, F_SETFL, fcntl(server, F_GETFL, 0) | O_NONBLOCK) != 0) {
		perror("fcntl failed");
		exit(1);
	}
	struct sockaddr_in addr;
	while (true){
		unsigned int size_of_addr = sizeof(addr);
		int fd = accept(server, (struct sockaddr*)&addr, &size_of_addr);
		if (fd != -1){
			fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
			appendClient(fd);
		}
		memset(&addr, 0, sizeof(addr)); // Clear that client address data - don't need it.
		struct client* c = first;
		struct client* last = c;
		while (c){
			if (doClientWork(c)){
				 close(c -> fd);
				 if (c == last){ // Splice it out
				 	first = c -> next;
				 }
				 else{
				 	last -> next = c -> next;
				 }
				 free(c);
			}
			c = c -> next;
		}
	}
	return 0;
}
