#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define BUFFER_SIZE	1024

int server_fd = 0;
pthread_mutex_t mutex;

// Hundler of the Signal SIGINT
void handleSIGINT() {
    // Fermer le socket du serveur avant de quitter
	printf("Marked the socket as socket who will be closed\n");
	int closed_network = shutdown(server_fd, SHUT_RDWR);
	if(closed_network == -1){
		printf("The close of the network failed: %s \n", strerror(errno));
		exit(1);
	}

	printf("Close the socket!\n");
	int close_socket = close(server_fd);
	if(close_socket == -1){
		printf("The close of the socket failed: %s \n", strerror(errno));
		exit(1);
	}
}

// Function who recover the path on the client HTTP request
char *request_path(char *buffer){
	// take the first line of the HTTP request
	char *first_line = strndup(buffer, strstr(buffer, "\r\n") - buffer);

	// remove the GET or the POST to the First line
	char *del_get = strstr(first_line, " ");

	// return the path on the first line of the HTTP request
	return strtok(del_get, " ");
}

// Function who extract the string on certains path (/echo/<string>)
char *extract_string(char *buffer){
	// move forward to remove the first "/"
	const char *substring = buffer+1;

	// find the first position of the second "/"
	char *slash_position = strchr(substring, '/');

	// return the string on the path
	return slash_position+1;
}

void *client_thread_handler(void *arg) {

	int client_accepted = *(int*)arg; 

	char client_response[BUFFER_SIZE];
	ssize_t message_recvd = recv(client_accepted, client_response, BUFFER_SIZE, 0);
	if(message_recvd == -1){
		printf("Message recvd fail: %s \n", strerror(errno));
		exit(1);
	}
	// Printf search the char '\0' on the end of the client_response
	// But the function recv() doesn't guarantee that the data we receve will end automaticaly with '\0'
	// That's why we can find some bizarre char on the printf next! 
	printf("Received: %zd\nbytes: \n%s\n", message_recvd, client_response);


	char *path = request_path(client_response);

	if(strcmp(path, "/") == 0 ){
		char *server_respond = "HTTP/1.1 200 ok\r\n\r\n";	
		ssize_t server_send_respond = send( client_accepted, server_respond, strlen(server_respond), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			exit(1);	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, server_respond);
		//After the send on this path, the client(if it's a browser) keep-alive the connexion with the server
		//that's why we can see the client is immédiatly connected after the send of the respond to the client

	}else if(strncmp(path, "/echo/", 6) == 0){ // test if the path contains the substring "/echo/"
		// extract the string we need on the path
		char *string = extract_string(path);
		size_t string_len = strlen(string);

		// Use the sprintf() to create the respond with all informations we have
		char resultat[BUFFER_SIZE];
		sprintf(resultat, "HTTP/1.1 200 ok\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", string_len, string);

		ssize_t server_send_respond = send( client_accepted, resultat, strlen(resultat), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			exit(1);	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, resultat);

	}else if (strncmp(path, "/user-agent", 11) == 0){
		char *userAgentHttp = strstr(client_response, "User-Agent:");
		char *userAgentLine = strndup(userAgentHttp, strstr(userAgentHttp, "\r\n") - userAgentHttp);
		char *userAgent = strstr(userAgentLine, " ")+1;
		size_t userAgentLen = strlen(userAgent);

		char resultatUserAgent[BUFFER_SIZE];
		sprintf(resultatUserAgent, "HTTP/1.1 200 ok\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", userAgentLen, userAgent);

		ssize_t server_send_respond = send( client_accepted, resultatUserAgent, strlen(resultatUserAgent), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			exit(1);	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, resultatUserAgent);

	}else{
		char *server_fail_respond = "HTTP/1.1 404 Not Found\r\n\r\n";
		ssize_t server_send_respond = send( client_accepted, server_fail_respond, strlen(server_fail_respond), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			exit(1);	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, server_fail_respond);

	}

	int close_acceptation = close(client_accepted);
	if(close_acceptation == -1){
		printf("The close of the acceptation failed: %s \n", strerror(errno));
		exit(1);
	}

	return NULL;
}

int main() {
	// Setting up the signal handler for SIGINT
	struct sigaction action;
    action.sa_handler = handleSIGINT;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	
	int client_addr_len;
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

	// when receve the signal SIGINT close properly the socket
	int sigaction_return = sigaction(SIGINT, &action, NULL);
	if (sigaction_return == -1) {
		printf("Error during the listing signal: %s \n", strerror(errno));
        return 1;
    }
	
	while (1){

		printf("Waiting for a client to connect...\n");
		client_addr_len = sizeof(client_addr);
		
		int pthread_mutex_lock_return = pthread_mutex_lock(&mutex);
		// if(pthread_mutex_lock_return != 0){
		// 	printf("The mutex was not locked: %s \n", strerror(errno));
		// 	return 1;
		// }
		// critical code
		int client_accepted = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		// end of the critical code
		int pthread_mutex_unlock_return = pthread_mutex_unlock(&mutex);
		// if(pthread_mutex_unlock_return != 0){
		// 	printf("The mutex was not unlocked: %s \n", strerror(errno));
		// 	return 1;
		// } 
		if(client_accepted == -1){
			printf("Client not connected: %s \n", strerror(errno));
			return 1;
		}

		printf("Client connected\n");

		pthread_t client_thread;
		int pthread_return = pthread_create(&client_thread, NULL, client_thread_handler, (void *)&client_accepted);
        if (pthread_return != 0) {
            perror("Erreur lors de la création du thread");

			int close_acceptation = close(client_accepted);
			if(close_acceptation == -1){
				printf("The close of the acceptation failed: %s \n", strerror(errno));
				return 1;
			}
            continue;
        }

		// wait for thread to finish
		int pthread_return_join = pthread_join(client_thread, NULL);
		if(pthread_return_join == -1){
			printf("Failed of the destruction of the thread\n");
			return 1;
		}
				
	}

	return 0;
}
