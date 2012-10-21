#ifndef _UTIL_H_
#define _UTIL_H_


#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <semaphore.h>


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

/* Multi-thread producer/consumer shared buffer */
/* $begin sbuf_t */
typedef struct {
    int *buf;       /* Buffer array (item is int type) */
    int n;          /* Max number of slots */
    int front;      /* buf[(front+1)%n] is the first item */
    int rear;       /* buf[rear%n] is the latst one */
    sem_t *mutex;   /* Protects accesses to buf */
    sem_t *slots;   /* Counts available slots */
    sem_t *items;   /* Counts available items */
} sbuf_t;
/* $end sbuf_t */

/* Misc constants */
#define MAXLINE  8192  /* max text line length */
#define MAXBUF   8192  /* max I/O buffer size */
#define LISTENQ  1024  /* second argument to listen() */

#define MAXOBJECTSIZE (1024 * 80)
#define MAXOBJECTCOUNT 128

/* Rio (Robust I/O) package */
extern ssize_t rio_readn(int fd, void *usrbuf, size_t n);
extern ssize_t rio_writen(int fd, void *usrbuf, size_t n);
extern void rio_readinitb(rio_t *rp, int fd);
extern ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);
extern ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);


/* POSIX semaphore wrappers */
void P(sem_t *sem);
void V(sem_t *sem);

/* Sbuf (Shared buffer) package */
extern void sbuf_init(sbuf_t *sp, int n);
extern void sbuf_free(sbuf_t *sp);
extern void sbuf_insert(sbuf_t *sp, int item);
extern int  sbuf_remove(sbuf_t *sp);

extern void cache_init();
extern char *cache_load(char *url);
extern int cache_insert(char *url, char *obj);

/* Error process function */
extern void error_exit(char *s);

#endif
