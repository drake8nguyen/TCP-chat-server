/* 
 * tcpclient.c - A simple TCP client
 * usage: tcpclient <host> <port>
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#define BUFSIZE 400


struct __attribute__((__packed__)) header {
  unsigned short int type; //2 bytes
  char source[20]; //20 bytes
  char destination[20]; //20 bytes
  unsigned int length; //4 bytes
  unsigned int message_id; //4 bytes
};
typedef struct header* header;


/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
}

void read_header(char buf[BUFSIZE], int sockfd, header read_hdr)
{
    bzero(buf, BUFSIZE);
    int n;
    n = read(sockfd, buf, 50); //you dont want to read in BUFSIZE here, know exactly how many you want to read
    //TCP is stream oriented, so it will keep reading until getting the whole BUFFSIZE = 400
    if (n < 0) 
      error("ERROR reading from socket");

    memcpy(read_hdr, buf, 50);

    fprintf(stderr, "HEADER TYPE: %d\n", ntohs(read_hdr->type));
    fprintf(stderr, "HEADER SOURCE: %s\n", read_hdr->source);
}

void write_header (char buf[BUFSIZE], int sockfd, header hdr)
{
    bzero(buf, BUFSIZE);
    memcpy(buf, hdr, 50);
    int n;
    n = write(sockfd, buf, 50);
    if (n < 0) 
      error("ERROR writing to socket");
    free(hdr);    
}

void write_hello_header(char buf[BUFSIZE], int sockfd) 
{
    header hello_hdr = malloc(50);
    hello_hdr->type = htons(1);
    memcpy(hello_hdr->source, "10ccccccc\0", sizeof("10ccccccc\0"));
    memcpy(hello_hdr->destination, "Server\0", sizeof("Server\0"));
    hello_hdr->length = htonl(0);
    hello_hdr->message_id = htonl(0);
    write_header(buf, sockfd, hello_hdr);
}


int main(int argc, char **argv) {
    int sockfd, portno, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
    (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);
    if (connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0) 
      error("ERROR connecting");


    write_hello_header(buf, sockfd);
    ////////////////////PRINT HELLO ACK HEADER
    header hello_ack_header = malloc(50);
    read_header(buf, sockfd, hello_ack_header);
    free(hello_ack_header);

    ///////////////////////print client list header
    header client_list_hdr = malloc(50);
    read_header(buf, sockfd, client_list_hdr);
    //print client list - not working, only print first client
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, ntohl(client_list_hdr->length));
    if (n < 0) 
        error("ERROR reading from socket");
    bzero(buf, BUFSIZE);
    free(client_list_hdr);

    // //////////////////////READ CHAT HEADER
    header chat_hdr = malloc(50);
    read_header(buf, sockfd, chat_hdr);
    free(chat_hdr);

    ////////////////////Read a chat message
    bzero(buf, BUFSIZE);
    n = read(sockfd, buf, chat_hdr->length);
    fprintf(stderr, "%s\n", buf);


    while (1);
    
    // sleep(1);
    // close(sockfd);
    return 0;
}
