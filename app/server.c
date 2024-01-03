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
#include <sys/stat.h>
#include <dirent.h>

#define BUFFER_SIZE	1024

struct server_to_thread {
	int client_socket;
	int argc;
	char *dir_path;
};


int client_count = 0;
int server_fd = 0;
pthread_mutex_t mutex;

// Hundler of the Signal SIGINT
void handleSIGINT() {
    // Fermer le socket du serveur avant de quitter
	printf("The Server was shutdown by CTRL+C\nMarked the socket as socket who will be closed\n");
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
    // Find the first occurrence of "\r\n" to identify the end of the first line
    const char *line_end = strstr(buffer, "\r\n");

    if (line_end != NULL) {
        // Calculate the length of the first line
        size_t line_length = line_end - buffer;

        // Allocate memory for the first line and copy it
        char *line = (char *)malloc(line_length + 1);
        strncpy(line, buffer, line_length);
        line[line_length] = '\0'; // Null-terminate the string

        // Find the first space to identify the end of the method and the beginning of the path
        const char *space = strchr(line, ' ');

        if (space != NULL) {
            // Move the pointer to the beginning of the path
            const char *path_start = space + 1;

            // Find the end of the path
            const char *path_end = strchr(path_start, ' ');

            if (path_end != NULL) {
                // Calculate the length of the path
                size_t path_length = path_end - path_start;

                // Allocate memory for the path and copy it
                char *path = (char *)malloc(path_length + 1);
                strncpy(path, path_start, path_length);
                path[path_length] = '\0'; // Null-terminate the string

                free(line); // Free the memory allocated for the first line
                return path;
            }
        }

        free(line); // Free the memory allocated for the first line
    }

	return NULL;
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

// Client thread handler! It's this functon who will execute when the main creat a thread!
void *client_thread_handler(void *arg) {

	struct server_to_thread thrd_arg = *(struct server_to_thread*)arg;

	char client_response[BUFFER_SIZE];
	ssize_t message_recvd = recv(thrd_arg.client_socket, client_response, BUFFER_SIZE, 0);
	if(message_recvd == -1){
		printf("Message recvd fail: %s \n", strerror(errno));
		pthread_exit((int *)1);
	}
	// Printf search the char '\0' on the end of the client_response
	// But the function recv() doesn't guarantee that the data we receve will end automaticaly with '\0'
	// That's why we can find some bizarre char on the printf next! 
	printf("Received: %zd\nbytes: \n%s\n", message_recvd, client_response);

	char *path = request_path(client_response);

	if(path != NULL && strcmp(path, "/") == 0 ){
		char *server_respond = "HTTP/1.1 200 ok\r\nConnection: close\r\n\r\n";	
		ssize_t server_send_respond = send( thrd_arg.client_socket, server_respond, strlen(server_respond), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			pthread_exit((int *)1);	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, server_respond);
		//After the send on this path, the client(if it's a browser) keep-alive the connexion with the server
		//that's why we can see the client is immédiatly connected after the send of the respond to the client

	}else if(path != NULL && strncmp(path, "/echo/", 6) == 0){ // test if the path contains the substring "/echo/"
		// extract the string we need on the path
		char *string = extract_string(path);
		size_t string_len = strlen(string);

		// Use the sprintf() to create the respond with all informations we have
		char resultat[BUFFER_SIZE];
		sprintf(resultat, "HTTP/1.1 200 ok\r\nContent-Type: text/plain\r\nConnection: close\r\nContent-Length: %ld\r\n\r\n%s", string_len, string);

		ssize_t server_send_respond = send( thrd_arg.client_socket, resultat, strlen(resultat), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			pthread_exit((int *)1);	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, resultat);

	}else if (path != NULL && strncmp(path, "/user-agent", 11) == 0){
		char *userAgentHttp = strstr(client_response, "User-Agent:");
		char *userAgentLine = strndup(userAgentHttp, strstr(userAgentHttp, "\r\n") - userAgentHttp);
		char *userAgent = strstr(userAgentLine, " ")+1;
		size_t userAgentLen = strlen(userAgent);

		char resultatUserAgent[BUFFER_SIZE];
		sprintf(resultatUserAgent, "HTTP/1.1 200 ok\r\nContent-Type: text/plain\r\nConnection: close\r\nContent-Length: %ld\r\n\r\n%s", userAgentLen, userAgent);

		ssize_t server_send_respond = send( thrd_arg.client_socket, resultatUserAgent, strlen(resultatUserAgent), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			pthread_exit((int *)1);	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, resultatUserAgent);

		//after check with valgrind
		free(userAgentLine);

	}else if (path != NULL && strncmp(path, "/files/", 7) == 0 && thrd_arg.argc == 3){
		// Recup le nom du fichier
		char *filename=extract_string(path);
		char directory[strlen(thrd_arg.dir_path)+strlen(filename)+1];

		printf("The dir where supposed to be the file : %s\n",thrd_arg.dir_path);
		printf("The file name : %s\n",filename);

		//fusionner le dir_path et le filename
		if(thrd_arg.dir_path[strlen(thrd_arg.dir_path)-1] != '/'){
			sprintf(directory,"%s/%s", thrd_arg.dir_path, filename);
		}else{
			sprintf(directory,"%s%s", thrd_arg.dir_path, filename);
		}
		
		printf("The absolute path for the file : %s\n",directory);

		struct stat path_stat;

        // Utilise stat pour obtenir des informations sur le chemin
        if (stat(directory, &path_stat) == 0) {
            // Vérifie si c'est un répertoire et qu'il existe
            // if (!S_ISREG(path_stat.st_mode)) {
            //     printf("The path don't mentionne a Dir\n");
            //     return 1;
            // }
			
			// lire le fichier 
			size_t size_buffer = path_stat.st_size;

			printf("The size of the file with path_stat : %ld\n",path_stat.st_size);
			printf("The size of the file with size_byffer : %ld\n",size_buffer);


			char buffer[size_buffer];

			FILE *file = fopen(directory, "rb");
			if (file != NULL) {
				fread(buffer, sizeof(char), size_buffer, file);

				if(ferror(file)){
					printf("Failed to read the file\n");
					pthread_exit((int *)1);	
				}
				
				//closed the string with the '\0'
				buffer[size_buffer]='\0';

				fclose(file);
			}

			printf("The file contains : %s\n",buffer);
			// construire la réponse avec les headers et le contenue du fichier
			// envoyer la réponse
			char resultatUserAgent[3*BUFFER_SIZE];
			sprintf(resultatUserAgent, "HTTP/1.1 200 ok\r\nContent-Type: application/octet-stream\r\nConnection: close\r\nContent-Length: %ld\r\n\r\n%s", size_buffer, buffer);

			ssize_t server_send_respond = send( thrd_arg.client_socket, resultatUserAgent, strlen(resultatUserAgent), 0);
			if(server_send_respond == -1){
				printf("The respond fail to be sended: %s \n", strerror(errno));
				pthread_exit((int *)1);	
			}
			printf("Send: %zd \nbytes: \n%s\n", server_send_respond, resultatUserAgent);
			
        }else{
			// envoyer une réponse 404
			char *server_fail_respond = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
			ssize_t server_send_respond = send( thrd_arg.client_socket, server_fail_respond, strlen(server_fail_respond), 0);
			if(server_send_respond == -1){
				printf("The respond fail to be sended: %s \n", strerror(errno));
				pthread_exit((int *)1);	
			}
			printf("Send: %zd \nbytes: \n%s\n", server_send_respond, server_fail_respond);

        }
	
	}else{
		char *server_fail_respond = "HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n";
		ssize_t server_send_respond = send( thrd_arg.client_socket, server_fail_respond, strlen(server_fail_respond), 0);
		if(server_send_respond == -1){
			printf("The respond fail to be sended: %s \n", strerror(errno));
			pthread_exit((int *)1);	
		}
		printf("Send: %zd \nbytes: \n%s\n", server_send_respond, server_fail_respond);

	}

	// after check with valgrind
	free(path);
	free(arg);

	printf("Marked the acceptation who will be closed\n");
	int shutdown_acceptation = shutdown(thrd_arg.client_socket, SHUT_RDWR);
	if(shutdown_acceptation == -1){
		printf("The close of the network failed: %s \n", strerror(errno));
		pthread_exit((int *)1);
	}

	printf("Close the acceptation!\n");
	int close_acceptation = close(thrd_arg.client_socket);
	if(close_acceptation == -1){
		printf("The close of the socket failed: %s \n", strerror(errno));
		pthread_exit((int *)1);
	}

	return NULL;
}

int main(int argc, char *argv[]) {
	char *dir_path=argv[2];

	//look for a argument
	if((argc == 2) || (argc > 3)){

        printf("Usage: ./server [--directory] [path]\n");
        return 1;

    }else if (argc == 3){

        if(strcmp(argv[1],"--directory") != 0){
            printf("Usage: ./server [--directory] [path]\n");
            return 1;
        }

        struct stat path_stat;

        if(dir_path[0] != '/'){
            printf("The path must be a absolute path\n");
            return 1;
        }

        // Utilise stat pour obtenir des informations sur le chemin
        if (stat(dir_path, &path_stat) == 0) {
            // Vérifie si c'est un répertoire et qu'il existe
            if (!S_ISDIR(path_stat.st_mode)) {
                printf("The path don't mentionne a Dir\n");
                return 1;
            }
        }else{
            printf("The Dir doesn't exist\n");
            return 1;
        }

    }

	// Setting up the signal handler for SIGINT
	struct sigaction action;
	socklen_t client_addr_len;
	struct sockaddr_in client_addr;
	client_addr_len = sizeof(client_addr);

    action.sa_handler = handleSIGINT;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;

	// Disable output buffering
	setbuf(stdout, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	
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

	//init the mutex
	if (pthread_mutex_init(&mutex, NULL) != 0) {
        printf("Error during initialisation of Mutex: %s\n", strerror(errno));
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

		int client_accepted = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		if(client_accepted == -1){
			printf("Client not connected: %s \n", strerror(errno));
			return 1;
		}

		struct server_to_thread *arg = malloc(sizeof(struct server_to_thread));

		pthread_mutex_lock(&mutex);
		// critical code
		if (argc==3){
			arg->dir_path=dir_path;
		}else{
			arg->dir_path=NULL;
		}
		arg->argc=argc;
        arg->client_socket = client_accepted;
		client_count++;
		printf("Client %d connected\n", client_count);
		// end of the critical code
		pthread_mutex_unlock(&mutex);

		pthread_t client_thread;
		int pthread_return = pthread_create(&client_thread, NULL, client_thread_handler, arg);
        if (pthread_return != 0) {
            perror("Erreur lors de la création du thread");

			int close_acceptation = close(client_accepted);
			if(close_acceptation == -1){
				printf("The close of the acceptation failed: %s \n", strerror(errno));
				return 1;
			}
            continue;
        }

		// detach the thread
		int pthread_return_detach = pthread_detach(client_thread);
		if(pthread_return_detach == -1){
			printf("Failed to detach the thread\n");
			return 1;
		}
	
	}

	printf("The Server was shutdown by ending the infinity loops\nMarked the socket as socket who will be closed\n");
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

	pthread_mutex_destroy(&mutex);

	return 0;
}
