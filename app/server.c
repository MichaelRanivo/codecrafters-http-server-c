#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define BUFFER_SIZE	1024

int path_valid(char *buffer, ssize_t bytes_read){

	char *get = "GET / HTTP/1.1\r\n";
	if(bytes_read < strlen(get)){
		return 0;
	}

	for(size_t i=0; i<strlen(get); i++){
		if(buffer[i] != get[i]){
			return 0;
		}
	}

	return 1;
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { 
		.sin_family = AF_INET ,
		.sin_port = htons(4221),
		.sin_addr = { htonl(INADDR_ANY) },
	};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	int client_accepted = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	if(client_accepted == -1){
		printf("Client not connected: %s \n", strerror(errno));
		return 1;
	}

	printf("Client connected\n");

	char client_response[BUFFER_SIZE];
	ssize_t message_recvd = recv(client_accepted, client_response, BUFFER_SIZE, 0);
	if(message_recvd == -1){
		printf("Message recvd fail: %s \n", strerror(errno));
		return 1;
	}
	printf("Received: %zd\nbytes: \n%s\n", message_recvd, client_response);

	char *server_respond = "HTTP/1.1 200 ok\r\n\r\n";
	char *server_fail_respond = "HTTP/1.1 404 Not Found\r\n\r\n";

	if(path_valid(client_response, message_recvd)){
		ssize_t server_send_respond = send( client_accepted, server_respond, strlen(server_respond), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			return 1;	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, server_respond);
	}else{
		ssize_t server_send_respond = send( client_accepted, server_fail_respond, strlen(server_fail_respond), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			return 1;	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, server_fail_respond);
	}
	
	close(server_fd);

	return 0;
}
