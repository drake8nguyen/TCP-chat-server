#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>

#define BUFFSIZE 400
#define HDR_SIZE 50
#define MAX_NO_CLIENTS 200
#define TIME_LIMIT 60



void error(char *msg) {
  perror(msg); 
  exit(EXIT_FAILURE);
}

struct __attribute__((__packed__)) header {
  unsigned short int type; //2 bytes
  char source[20]; //20 bytes
  char destination[20]; //20 bytes
  unsigned int length; //4 bytes
  unsigned int message_id; //4 bytes
};
typedef struct header* header;

struct client_list {
  char ids[MAX_NO_CLIENTS][20]; 
  char socks[MAX_NO_CLIENTS];   
  int no_clients;
}; 
typedef struct client_list* client_list;

struct partial_buffer {
	char buf[BUFFSIZE];
	int client_sock;
	int curr_size;
	int remaining;
	int true_length;
	int is_header;
	header sender_hdr; 
	time_t idle_period; 

};
typedef struct partial_buffer* partial_buffer;

void message_type_handler(char buffer[BUFFSIZE], client_list, int, partial_buffer*, fd_set*, int*);


void exit_handler(client_list clients, int clientfd, fd_set* master_fd_set)
{
	close(clientfd);
	FD_CLR(clientfd, master_fd_set);
	if (clients->no_clients == 1) {
		clients->socks[0] = -1;
		memset(clients->ids[0], 0, 20);
	} else {
		for (int i = 0; i < clients->no_clients; i++) {
			if (clients->socks[i] == clientfd) {
		        for(int j = i; j < clients->no_clients-1; j++)
		            clients->socks[j] = clients->socks[j + 1];
		        for(int k = i; k < clients->no_clients-1; k++)
					memcpy(clients->ids[k], clients->ids[k+1], strlen(clients->ids[k+1])+1);
		        break;
			}
		}
	}
	clients->no_clients--;
}

void send_header (char buffer[BUFFSIZE], client_list clients, int clientfd, header incoming_header, int type) 
{
	header outgoing_header = malloc (HDR_SIZE);

	outgoing_header->type = htons(type);

	if ((type == 7) || (type == 2) || (type == 8)) { 
		memcpy(outgoing_header->source, "Server\0", sizeof("Server\0"));
		memcpy(outgoing_header->destination, incoming_header->source, sizeof(incoming_header->source));
		outgoing_header->length = htonl(0);
		if (type == 8)
			outgoing_header->message_id = htonl(incoming_header->message_id); 
		else
			outgoing_header->message_id = htonl(0);
	} else if (type == 4) { 
		memcpy(outgoing_header->source, "Server\0", sizeof("Server\0"));
		memcpy(outgoing_header->destination, incoming_header->source, sizeof(incoming_header->source));
		outgoing_header->length = 0;
		for (int i = 0; i < clients->no_clients; i++)
			outgoing_header->length += (strlen(clients->ids[i]) + 1);
		outgoing_header->length = htonl(outgoing_header->length); 
		outgoing_header->message_id = htonl(0);
	} else if (type == 5) { 
		bzero(buffer, BUFFSIZE);
		memcpy(buffer, incoming_header, HDR_SIZE);
		int n = write(clientfd, buffer, HDR_SIZE); 
		if (n < 0)
			error("ERROR writing to socket");
		bzero(buffer, BUFFSIZE);
		free(outgoing_header);
		return;
	}
	bzero(buffer, BUFFSIZE);
	memcpy(buffer, outgoing_header, HDR_SIZE);

	int n = write(clientfd, buffer, HDR_SIZE);
	if (n < 0)
		error("ERROR writing to socket");
	bzero(buffer, BUFFSIZE);
	free(outgoing_header);
	return;
}

void list_request_handler (char buffer[BUFFSIZE], client_list clients, int clientfd, header incoming_header)
{
	header outgoing_header = malloc (HDR_SIZE);
	//Send back header of client lists
	send_header (buffer, clients, clientfd, incoming_header, 4);

	//Send back data of client lists
	bzero(buffer, BUFFSIZE);
	memcpy(buffer, clients->ids[0], strlen(clients->ids[0]) + 1); //dont want the whole 20 bytes -> dont use sizeof()
	int curr_list_length = strlen(clients->ids[0]) + 1;
	for (int i = 1; i < clients->no_clients; i++) {
		memcpy(buffer + curr_list_length, clients->ids[i], strlen(clients->ids[i]) + 1); //can also use strchr()
		curr_list_length += strlen(clients->ids[i]) + 1;
		//fprintf(stderr, "Curr length is %d\n", curr_list_length);
	}
	int n = write(clientfd, buffer, curr_list_length);
	if (n < 0)
		error("ERROR writing to socket");

	bzero(buffer, BUFFSIZE);
}


void hello_handler(char buffer[BUFFSIZE], client_list clients, int clientfd, header incoming_header, fd_set* master_fd_set) 
{
	//Check if client is already in list
	for (int i = 0; i < clients->no_clients; i++) {

		if (!strcmp(clients->ids[clients->no_clients - 1], incoming_header->source)) {
			/*If found, SEND ERROR, CLOSE TCP*/
			send_header(buffer, clients, clientfd, incoming_header, 7); //need some sort of error message?
			sleep(5); //to allow the client to read the header before closing the sock
			exit_handler(clients, clientfd, master_fd_set);

			return;
		}
	}

	memcpy(clients->ids[clients->no_clients], incoming_header->source, strlen(incoming_header->source)+1);
	clients->socks[clients->no_clients] = clientfd;
	clients->no_clients++;

	//Send back HELLO_ACK
	send_header(buffer, clients, clientfd, incoming_header, 2);
	//Send back client list
	list_request_handler(buffer, clients, clientfd, incoming_header);
}


void find_recipient_forward_data (char buffer[BUFFSIZE], client_list clients, int clientfd, header sender_header, fd_set* master_fd_set)
{	
	int k;
	for (k = 0; k < clients->no_clients; k++)
		if (!strcmp(sender_header->destination, clients->ids[k]))
			break;

	if (k == clients->no_clients) { 
		fprintf(stderr, "SEND ERROR 8\n");
		send_header(buffer, clients, clientfd, sender_header, 8);
		return;
	} else {
		unsigned int data_length = ntohl(sender_header->length);
		int n;
		n = write (clients->socks[k], buffer, data_length);
		bzero(buffer, BUFFSIZE);
	 	if (n < 0) 
	 		error("Error forwarding message");
	   	fprintf(stderr, "Forwarded: %s\n", buffer);
   	}
}


void partial_data_handler (char buffer[BUFFSIZE], client_list clients, int clientfd, header sender_header, int data, partial_buffer* partialList, fd_set* master_fd_set, int n, int expected_length, int* partialListSize)
{
	int j;
	int exist = 0;
		if ((*partialListSize) == 0) {
			partialList[0] = realloc(partialList[0], sizeof(struct partial_buffer));
			memcpy (partialList[0]->buf, buffer, n); 

			partialList[0]->curr_size = n; //reset to 0
			partialList[0]->remaining = expected_length - n;
			partialList[0]->true_length = expected_length;

			partialList[0]->client_sock = clientfd;
			partialList[0]->idle_period = 0;
			partialList[0]->sender_hdr = malloc(HDR_SIZE);

			if (data == -1) {
				partialList[0]->is_header = 1;
				fprintf(stderr, "FIRST NODE (HEADER) %d bytes at sock %d\n", partialList[0]->curr_size, clientfd); 
			}
			else {
				partialList[0]->is_header = 0;
				memcpy(partialList[0]->sender_hdr, sender_header, HDR_SIZE);
				fprintf(stderr, "FIRST NODE (DATA) %d bytes at sock %d\n", partialList[0]->curr_size, clientfd); 
			}

			(*partialListSize)++;
		} else {
			for (j = 0; j < (*partialListSize); j++) {
				fprintf(stderr, "Size of node %d is %d\n", j, partialList[j]->curr_size);
				fprintf(stderr, "Header or Data %d\n", partialList[j]->is_header);
				if (partialList[j]->client_sock == clientfd) {
					exist = 1;
					memcpy (partialList[j]->buf + partialList[j]->curr_size, buffer, n);
					partialList[j]->curr_size += n;
					partialList[j]->remaining -= n;
					partialList[j]->idle_period = 0;

					if (partialList[j]->curr_size == partialList[j]->true_length) {
						bzero(buffer, BUFFSIZE);
						memcpy(buffer, partialList[j]->buf, expected_length);
						if (partialList[j]->is_header == 1) {
							fprintf(stderr, "FINISHED COMBINING %d bytes of HEADER at socket %d\n", partialList[j]->curr_size, clientfd);
							message_type_handler(buffer, clients, clientfd, partialList, master_fd_set, partialListSize);
						}
						else {
							fprintf(stderr, "FINISHED COMBINING %d bytes of DATA at socket %d\n", partialList[j]->curr_size, clientfd);
							bzero(buffer, BUFFSIZE);
							memcpy(buffer, partialList[j]->buf, partialList[j]->true_length);
				  			find_recipient_forward_data(buffer, clients, clientfd, partialList[j]->sender_hdr, master_fd_set);
						}
						if ((*partialListSize) == 1) {
							fprintf(stderr, "***********Delete the only node at sock %d\n", partialList[j]->client_sock);
							free(partialList[j]);
							fprintf(stderr, "This node after delete: sock %d true_length %d**********\n", partialList[j]->client_sock, partialList[j]->true_length);							
						}
						else {
							fprintf(stderr, "*************\nDelete node at sock %d\n", clientfd);
							int k;				
			        		for(k = j; k < (*partialListSize)-1; k++) {
			        			fprintf(stderr, "SHIFT OVER sock %d, true length %d\n", partialList[k+1]->client_sock, partialList[k+1]->true_length);
								memcpy(partialList[k]->buf, partialList[k+1]->buf, sizeof(partialList[k+1]->buf));
								partialList[k]->true_length = partialList[k+1]->true_length;
								partialList[k]->curr_size = partialList[k+1]->curr_size;
								partialList[k]->client_sock= partialList[k+1]->client_sock;
								partialList[k]->remaining = partialList[k+1]->remaining;
								partialList[k]->is_header = partialList[k+1]->is_header;
								partialList[k]->idle_period = partialList[k+1]->idle_period;
								memcpy(partialList[k]->sender_hdr, partialList[k+1]->sender_hdr, HDR_SIZE);

							fprintf(stderr, "This node after delete: sock %d true_length %d\n**************\n", partialList[k]->client_sock, partialList[k]->true_length);
			        		}
			        		free(partialList[k]);
						}
						(*partialListSize)--;
						fprintf(stderr, "Partial list size AFTER %d\n", (*partialListSize));
					}
					break;
				}
			}
			if (!exist) {
				partialList[(*partialListSize)] = malloc(sizeof(struct partial_buffer));
				(*partialListSize)++;
				memcpy (partialList[(*partialListSize)-1]->buf, buffer, n);

				partialList[(*partialListSize)-1]->curr_size = n;
				partialList[(*partialListSize)-1]->remaining = expected_length - n;
				partialList[(*partialListSize)-1]->true_length = expected_length;

				partialList[(*partialListSize)-1]->client_sock = clientfd;
				partialList[(*partialListSize)-1]->idle_period = 0;		
				partialList[(*partialListSize)-1]->sender_hdr = malloc(HDR_SIZE);

				if (data == -1) {
					partialList[(*partialListSize)-1]->is_header = 1;
				}
				else {
					memcpy(partialList[(*partialListSize)-1]->sender_hdr, sender_header, HDR_SIZE);
					partialList[(*partialListSize)-1]->is_header = 0;
				}
				bzero(buffer, BUFFSIZE); 
			}
		}
	bzero(buffer, BUFFSIZE);

}

void chat_handler(char buffer[BUFFSIZE], client_list clients, int clientfd, header sender_header, partial_buffer* partialList, fd_set* master_fd_set, int* partialListSize) 
{
	int i;
	if (!strcmp(sender_header->source, sender_header->destination)) {
		exit_handler(clients, clientfd, master_fd_set);
		return;
	}

	for (i = 0; i < clients->no_clients; i++)
		if (!strcmp(sender_header->source, clients->ids[i])) {
			break;
		}

	if (i == clients->no_clients) {
		close(clientfd);
		FD_CLR(clientfd, master_fd_set);
		return;
	}

	header outgoing_header = malloc (HDR_SIZE);
	unsigned int expected_length = ntohl(sender_header->length);
	for (i = 0; i < clients->no_clients; i++)
		if (!strcmp(sender_header->destination, clients->ids[i])) { 
			send_header(buffer, clients, clients->socks[i], sender_header, 5);
			break;
		} 


	//processing data
 	bzero(buffer, BUFFSIZE);
  	int n = read (clientfd, buffer, expected_length);
  	fprintf(stderr, "Bytes read for data %d\n", n);
  	fprintf(stderr, "To forward: %s\n", buffer);
 	if (n < 0) 
		error("Error receiving message");
  	else if (n < expected_length)
  		partial_data_handler(buffer, clients, clientfd, sender_header, i, partialList, master_fd_set, n, expected_length, partialListSize);
  	else if (n == expected_length) 
  		find_recipient_forward_data(buffer, clients, clientfd, sender_header, master_fd_set);
}

void message_type_handler(char buffer[BUFFSIZE], client_list clients, int clientfd, partial_buffer* partialList, fd_set* master_fd_set, int* partialListSize)
{
	header incoming_header = malloc(HDR_SIZE);
	memcpy(incoming_header, buffer, HDR_SIZE);
	bzero(buffer, BUFFSIZE); 
	unsigned short int header_type = ntohs(incoming_header->type);
	fprintf(stderr, "INCOMING HEADER TYPE %d\n", header_type);
	if (header_type == 1) {
	  hello_handler(buffer, clients, clientfd, incoming_header, master_fd_set);
	} else if (header_type == 3) { 
		list_request_handler(buffer, clients, clientfd, incoming_header);
	} else if (header_type == 5) {
		chat_handler(buffer, clients, clientfd, incoming_header, partialList, master_fd_set, partialListSize);
	} else if (header_type == 6) {
	  	exit_handler(clients, clientfd, master_fd_set);
	} else {
		exit_handler(clients, clientfd, master_fd_set);	
	}
}

int read_header(char buffer[BUFFSIZE], client_list clients, int clientfd, partial_buffer* partialList, fd_set* master_fd_set, int* partialListSize) 
{
  	bzero(buffer, BUFFSIZE);
  	int n = read (clientfd, buffer, HDR_SIZE);
 	fprintf(stderr, "Bytes Read for header %d\n", n);
 	header dummy;
	if (n <= 0) {
		exit_handler(clients, clientfd, master_fd_set);
		return -1;

	}

	else if (n < HDR_SIZE) 
		partial_data_handler(buffer, clients, clientfd, dummy, -1, partialList, master_fd_set, n, HDR_SIZE, partialListSize);
	else if (n == HDR_SIZE) {
		header temp = malloc(HDR_SIZE);
		memcpy(temp, buffer, HDR_SIZE);
		unsigned short int check_type = ntohs(temp->type);
		if ((check_type == 1) || (check_type == 3) || (check_type == 5) || (check_type == 6))
			message_type_handler (buffer, clients, clientfd, partialList, master_fd_set, partialListSize);
		else
			partial_data_handler(buffer, clients, clientfd, dummy, -1, partialList, master_fd_set, n, HDR_SIZE, partialListSize);
		free(temp);
	}

}

int make_socket (uint16_t port) 
{
  int sock;
  struct sockaddr_in serveraddr;

  /* Create the socket. */
  sock = socket (AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
	perror ("socket");
	exit (EXIT_FAILURE);
  }

  /* Give the socket a name. */
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons (port);
  serveraddr.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &serveraddr, sizeof (serveraddr)) < 0) {
	  perror ("ERROR on binding");
	  exit (EXIT_FAILURE);
	}

	if (listen (sock, 10) < 0)
	{
	  perror ("ERROR on listen");
	  exit (EXIT_FAILURE);
	} //so far only added main socket
  return sock;
}

int new_connection_handler(int listen_sock)
{
	struct sockaddr_in clientname; 
	size_t size = sizeof (clientname); 
	int new_client_sock = accept (listen_sock, (struct sockaddr *) &clientname, (int*) &size);
	if (new_client_sock < 0)
		error ("ACCEPT FAILURE");
	fprintf (stderr, "Server: connect from host %s, port %hd.\n", inet_ntoa (clientname.sin_addr), ntohs (clientname.sin_port));
	return new_client_sock;
}


