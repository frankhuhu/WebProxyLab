#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include "util.h"

#define MAXOBJECTSIZE 102400

typedef struct {
    char method[128];
    char uri[MAXLINE];
    char version[128];
    char host[MAXLINE];
    int port;
    char raw_str[5 * MAXLINE];
} request_t;

typedef struct {
    char header[MAXLINE];
    char *content;
    int  content_size;
} response_t;


int  create_clientfd(char *hostname, int port);
int  create_listenfd(int port);
void echo_error(char *s);
int  work(int clientfd);
void read_http_request(int clientfd, rio_t *rio, request_t *req);


int create_clientfd(char *hostname, int port) {
    int fd;
    struct hostent *hp;
    struct sockaddr_in servaddr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        echo_error("Failed to create client socket.");
        return -1;
    }

    if ((hp = gethostbyname(hostname)) == NULL) {
        echo_error("Error in gethostbyname.");
        return -1;
    }

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr, (char *)&servaddr.sin_addr.s_addr, hp->h_length);
    servaddr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        echo_error("Error in connect.");
        return -1;
    }

    return fd;
}

int create_listenfd(int port) {
    int fd, optval = 1;
    struct sockaddr_in servaddr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        echo_error("Failed to create listening socket.");
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int)) < 0) {
        echo_error("Error in setsockopt.");
        return -1;
    }

    bzero((char *)&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons((unsigned short)port);
    if (bind(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        echo_error("Error in bind.");
        return -1;
    }

    if (listen(fd, LISTENQ) < 0) {
        echo_error("Error in listen.");
        return -1;
    }

    return fd;
}

void echo_error(char *s) {
    fprintf(stderr, "%s\n", s);
}

void read_http_request(int clientfd, rio_t *rio, request_t *req) {
    int n;
    char buf[MAXLINE];

    strcpy(req->raw_str, "\0");

    rio_readlineb(rio, buf, MAXLINE);
    strcat(req->raw_str, buf);
    sscanf(buf, "%s %s %s", req->method, req->uri, req->version);
    if (strcasecmp(req->method, "GET")) {
        fprintf(stderr, "Proxy failed to handle non GET HTTP methods.\n");
        return;
    }

    req->port = 80;

    // read the rest of HTTP header.
    while ((n = rio_readlineb(rio, buf, MAXLINE)) != 0) {
#ifdef DEBUG
        printf("%s", buf);
#endif
        strcat(req->raw_str, buf);
        if (!strncmp(buf, "Host:", 5))
            sscanf(buf, "Host: %s", req->host);
        if (!strcmp(buf, "\r\n"))
            break;
    }
    strcat(req->raw_str, "\r\n");

#ifdef DEBUG
    printf("method = %s, uri = %s, version = %s, host = %s\n",
            req->method, req->uri, req->version, req->host);
#endif
}


int work(int clientfd) {
    int serverfd;
    rio_t serv_rio;
    rio_t cli_rio;
    int nread, len;
    char response_buf[MAXOBJECTSIZE];

    request_t *request;
    response_t *response;

    request = (request_t *)malloc(sizeof(request_t));
    response = (response_t *)malloc(sizeof(response_t));

    rio_readinitb(&cli_rio, clientfd);
    read_http_request(clientfd, &cli_rio, request);

    if ((serverfd = create_clientfd(request->host, request->port)) < 0) {
        echo_error("Error in create server socket.");
        goto handle_error;
    }

    rio_readinitb(&serv_rio, serverfd);
    if (rio_writen(serverfd, request->raw_str, strlen(request->raw_str)) < 0) {
        echo_error("Error in sending request.");
        goto handle_error;
    }

    nread = 0;
    memset(response_buf, 0, sizeof(response_buf));
    while ((len = rio_readnb(&serv_rio, response_buf, sizeof(response_buf))) > 0) {
        len += nread;
        if (rio_writen(clientfd, response_buf, len) < 0) {
            echo_error("Error in sending response to client.");
            goto handle_error;
        }
        memset(response_buf, 0, sizeof(response_buf));
    }

    free(request);
    free(response);
    close(serverfd);

    return 0;

handle_error:
    free(request);
    free(response);
    close(serverfd);
    return -1;
}

int main(int argc, char **argv)
{
    int port, listenfd, clientfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;

    if (argc != 2) {
        echo_error("usage: proxy <port>");
        exit(-1);
    }

    port = atoi(argv[1]);
    listenfd = create_listenfd(port);

    while (1) {
        clientlen = sizeof(clientaddr);
        if ((clientfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) < 0) {
            echo_error("Error in accept.");
            exit(-1);
        }

        work(clientfd);

        close(clientfd);
    }

    return 0;
}
