#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <pthread.h>
#include "util.h"


#define NTHREADS  20
#define SBUFSIZE  80
#define MAXOBJECTSIZE (1024 * 120)

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

sbuf_t sbuf;    /* Shared buffer of connected descriptors. */

int  create_clientfd(char *hostname, int port);
int  create_listenfd(int port);
void echo_error(char *s);
int  work(int clientfd);
void read_http_request(int clientfd, rio_t *rio, request_t *req);
void *thread_entry(void *vargp);


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
    char *buf;

    buf = (char *)malloc(sizeof(char) * MAXLINE);
    strcpy(req->raw_str, "\0");

    rio_readlineb(rio, buf, MAXLINE);
    strcat(req->raw_str, buf);
    sscanf(buf, "%s %s %s", req->method, req->uri, req->version);
    if (strcasecmp(req->method, "GET")) {
        echo_error("Proxy failed to handle non GET HTTP methods.\n");
        return;
    }

    req->port = 80;

    // read the rest of HTTP header.
    while ((n = rio_readlineb(rio, buf, MAXLINE)) != 0) {
        strcat(req->raw_str, buf);
        if (!strncmp(buf, "Host:", 5))
            sscanf(buf, "Host: %s", req->host);
        if (!strcmp(buf, "\r\n"))
            break;
#ifdef DEBUG
        printf("%s", buf);
#endif
    }
    strcat(req->raw_str, "\r\n");

#ifdef DEBUG
    printf("method = %s, uri = %s, version = %s, host = %s\r\n\r\n",
            req->method, req->uri, req->version, req->host);
#endif

    free(buf);
}


int work(int clientfd) {
    int serverfd;
    rio_t serv_rio;
    rio_t cli_rio;
    int nread, resp_len;
    char *response_buf;

    request_t *request;
    response_t *response;

    request = (request_t *)malloc(sizeof(request_t));
    response = (response_t *)malloc(sizeof(response_t));
    response_buf = (char *)malloc(sizeof(char) * MAXOBJECTSIZE);

    rio_readinitb(&cli_rio, clientfd);
    read_http_request(clientfd, &cli_rio, request);

    if ((serverfd = create_clientfd(request->host, request->port)) < 0) {
        echo_error("Error in create server socket.");
        goto work_failed;
    }

    rio_readinitb(&serv_rio, serverfd);
    if (rio_writen(serverfd, request->raw_str, strlen(request->raw_str)) < 0) {
        echo_error("Error in sending request.");
        goto work_failed;
    }

    nread = resp_len = 0;
//    memset(response_buf, 0, sizeof(char) * MAXOBJECTSIZE);
    while ((nread = rio_readnb(&serv_rio, response_buf, sizeof(char) * MAXOBJECTSIZE)) > 0) {
        resp_len += nread;
#ifdef DEBUG
        printf("%d bytes read from server.\n", nread);
#endif
        if (rio_writen(clientfd, response_buf, nread) < 0) {
            echo_error("Error in sending response to client.");
            goto work_failed;
        }
#ifdef DEBUG
        printf("%d bytes write to client.\n", nread);
#endif
//        memset(response_buf, 0, sizeof(char) * MAXOBJECTSIZE);
    }

work_success:
    free(request);
    free(response);
    free(response_buf);
    close(serverfd);

    return 0;

work_failed:
    free(request);
    free(response);
    free(response_buf);
    close(serverfd);
    return -1;
}

void *thread_entry(void *vargp) {
    if (pthread_detach(pthread_self()) < 0) {
        echo_error("Error in pthread_detach.");
        exit(-1);
    }

    while (1) {
        int connfd = sbuf_remove(&sbuf);
        work(connfd);
        close(connfd);
    }
}

int main(int argc, char **argv)
{
    int i, port, listenfd, clientfd;
    socklen_t clientlen;
    struct sockaddr_in clientaddr;
    pthread_t tid;

    if (argc != 2) {
        echo_error("usage: proxy <port>");
        exit(-1);
    }

    port = atoi(argv[1]);
    sbuf_init(&sbuf, SBUFSIZE);
    clientlen = sizeof(clientaddr);
    listenfd = create_listenfd(port);

    for (i = 0; i < NTHREADS; i++) {
        if (pthread_create(&tid, NULL, thread_entry, NULL) < 0) {
            echo_error("Error in pthread_create.");
            exit(-1);
        }
    }

    while (1) {
        if ((clientfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) < 0) {
            echo_error("Error in accept.");
            exit(-1);
        }
        sbuf_insert(&sbuf, clientfd);
    }

    sbuf_free(&sbuf);
    return 0;
}
