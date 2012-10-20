#ifndef _UTIL_H_
#define _UTIL_H_


#include <unistd.h>
#include <errno.h>
#include <sys/types.h>


/* Persistent state for the robust I/O (Rio) package */
/* $begin rio_t */
#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;                /* descriptor for this internal buf */
    int rio_cnt;               /* unread bytes in internal buf */
    char *rio_bufptr;          /* next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* internal buffer */
} rio_t;
/* $end rio_t */

/* Misc constants */
#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */


extern ssize_t rio_readn(int fd, void *usrbuf, size_t n);
extern ssize_t rio_writen(int fd, void *usrbuf, size_t n);
extern void rio_readinitb(rio_t *rp, int fd);
extern ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
extern ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);

#endif
