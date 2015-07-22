#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#define PORT 80 // HTTP
#define MAX_SERVERS 10

const char *servers[MAX_SERVERS]; // Array of provided servers
int current = 0; // Currently used server (number)

/* Get server's address for next client */
in_addr_t get_next_server_addr(void);

/* Add passed servers' IPs to array */ 
void manage_servers(int argc, char *argv[]);

int main(int argc, char *argv[]) {
	int res;
	manage_servers(argc, argv);

	struct sockaddr_in listener_addr;
	int listener_sock = socket(AF_INET, SOCK_STREAM, 0);

	listener_addr.sin_family = AF_INET;
	listener_addr.sin_port = htons(PORT);
	listener_addr.sin_addr.s_addr = INADDR_ANY;

	res = bind(listener_sock, (struct sockaddr *)&listener_addr, sizeof(listener_addr));
	if(res)
		printf("\nError in binding - first available port will be used\n");

	res = listen(listener_sock, 100);
	if(res)
		printf("Error in listening\n");

	/* XXX Print assigned PORT */
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	if (getsockname(listener_sock, (struct sockaddr *)&sin, &len) == -1)
		perror("getsockname");
	else
		printf("Port number: %d\n\n", ntohs(sin.sin_port));
	/* END */

	for(;;) {
		pid_t pid;
		int client_sock = accept(listener_sock, NULL, NULL);
		in_addr_t next_server_addr = get_next_server_addr();
		
		pid = fork();
		if(pid == 0) {
			printf("Client accepted\n");
			int data_s;

			int cur_size = 10;
			char *header = malloc(cur_size);
			char *header_pointer = header;
			for(;;) {
				data_s = recv(client_sock, header_pointer, 5, 0);
				header_pointer += data_s;
				if(header_pointer - header >= cur_size - 5) {
					cur_size += 10;
					int offset = header_pointer - header;
					header = realloc(header, cur_size);
					header_pointer = header + offset;
				}

				if(header_pointer - header >= 3) {
					int i2, flag = 0;
					for(i2=3; i2<=(header_pointer - header); i2++)
						if(header[i2-3] == '\r' && header[i2-2] == '\n' && header[i2-1] == '\r' && header[i2] == '\n') {
							if(i2 == cur_size)
								header = realloc(header, cur_size+1);
							header[i2+1] = '\0';
							flag = 1;
							break;
						}
					if(flag)
						break;
				}
			}

			struct sockaddr_in out_sock_addr;
			memset(&out_sock_addr, 0, sizeof(out_sock_addr));
			
			out_sock_addr.sin_family = AF_INET;
			out_sock_addr.sin_port = htons(PORT);
			out_sock_addr.sin_addr.s_addr = next_server_addr;

			int out_sock;
			out_sock = socket(AF_INET, SOCK_STREAM, 0);
			res = connect(out_sock, (struct sockaddr *)&out_sock_addr, sizeof(out_sock_addr));

			if(res)
				printf("Error in connecting\n");
			else {
				char server_addr[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &(out_sock_addr.sin_addr), server_addr, INET_ADDRSTRLEN);
				printf("Client forwarded to %s\n\n", server_addr);
			}

			send(out_sock, header, strlen(header), 0);

			void * data = malloc(1000);
			for(;;) {
				data_s = recv(out_sock, data, 1000, 0);
				if(data_s <= 0) //end or error
					break;
				send(client_sock, data, data_s, 0);
			}
						
			shutdown(out_sock, 2);
			shutdown(client_sock, 2);
			close(out_sock);
			close(client_sock);
			exit(0);
		}
	} 
	return 0;
}

void manage_servers(int argc, char *argv[]) {
	int i;

	if( (argc - 1) > MAX_SERVERS ) 
		printf("Too many servers provided,passed_args first %d will be used only\n", MAX_SERVERS);

	for (i = 1; i <= argc; i++) {
		servers[i - 1] = argv[i];

		if (i > (MAX_SERVERS + 1) )
			break;
	}

	for (i = 0; i < MAX_SERVERS; i++) {
		printf("Server[%d]: %s\n", i, servers[i]);
	}

	return;
}

in_addr_t get_next_server_addr(void) {
	in_addr_t next;

	if (servers[current] == NULL || current == MAX_SERVERS) {
		current = 0;
		next = inet_addr(servers[0]);
	}
	else
		next = inet_addr(servers[current]);
	
	current++;

	return next;
}

