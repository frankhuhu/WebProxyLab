#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include "util.h"


#define MAXHASH 999979

struct list_node;
struct hash_node;

typedef struct list_node {
    char *url;
    char obj[MAXOBJECTSIZE];
    struct hash_node *hashnode;
    struct list_node *prev;
    struct list_node *next;
} list_node_t;

typedef struct hash_node {
    list_node_t *p_list_node;
    int valid;
} hash_node_t;


hash_node_t hash_table[MAXHASH];
list_node_t *list_node_head, *list_node_tail;
int cache_items;
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

// ELF Hash Function
unsigned int ELFHash(char *str)
{
    unsigned int hash = 0;
    unsigned int x  = 0;

    while (*str)
    {
        hash = (hash << 4) + (*str++);
        if ((x = hash & 0xF0000000L) != 0)
        {
            hash ^= (x >> 24);
            hash &= ~x;
        }
    }

    return (hash & 0x7FFFFFFF);
}



static list_node_t* add_list_node(char *url, char *obj, hash_node_t *hnode) {
    list_node_t *plnode;

    plnode = (list_node_t *) malloc(sizeof(list_node_t));
    if (!plnode)
        return NULL;

    plnode->url = (char *) malloc(sizeof(char) * strlen(url) + 1);
    if (!plnode->url)
        return NULL;

    strcpy(plnode->url, url);
    memcpy(plnode->obj, obj, MAXOBJECTSIZE);
    plnode->hashnode = hnode;

    plnode->prev = NULL;
    plnode->next = list_node_head;
    if (list_node_head)
        list_node_head->prev = plnode;
    list_node_head = plnode;
    if (!list_node_tail)
        list_node_tail = plnode;
    cache_items++;

    return plnode;
}

static void adjust_list_node(list_node_t *plnode) {
    assert(plnode);

    // this node is the last node
    if (!plnode->next)
        list_node_tail = plnode->prev;

    if (plnode->prev)
        plnode->prev->next = plnode->next;
    if (plnode->next)
        plnode->next->prev = plnode->prev;

    plnode->prev = NULL;
    plnode->next = list_node_head;
    if (list_node_head)
        list_node_head->prev = plnode;
    list_node_head = plnode;
}

static void remove_list_node(list_node_t *plnode) {
    assert(plnode);
    // This node must be the tail node;
    assert(plnode == list_node_tail);

    if (plnode->prev)
        plnode->prev->next = NULL;
    list_node_tail = plnode->prev;
    plnode->hashnode->valid = 0;

    free(plnode);
}

static int hash_lookup(int hashcode, char *url) {
    int i;
    assert(0 <= hashcode && hashcode < MAXHASH);
    for (i = hashcode; (i+1)%MAXHASH != hashcode; i = (i+1)%MAXHASH) {
        if (hash_table[i].valid) {
            if (!strcmp(hash_table[i].p_list_node->url, url))
                return i;
        } else {
            break;
        }
    }

    return -1;
}

static int hash_insert(int hashcode, char *url, char *obj) {
    int i;
    assert(0 <= hashcode && hashcode < MAXHASH);
    for (i = hashcode; (i+1)%MAXHASH != hashcode; i = (i+1)%MAXHASH) {
        if (!hash_table[i].valid) {
            list_node_t *p = add_list_node(url, obj, &hash_table[i]);
            if (!p)
                return -1;
            hash_table[i].valid = 1;
            hash_table[i].p_list_node = p;
            return i;
        }
    }
    return -1;
}

void cache_init() {
    int i;
    for (i = 0; i < MAXHASH; i++) {
        hash_table[i].p_list_node = NULL;
        hash_table[i].valid = 0;
    }
    list_node_head = list_node_tail = NULL;
    cache_items = 0;
}

char *cache_load(char *url) {
    int i, hashcode;
    char *obj = NULL;

    pthread_mutex_lock(&cache_mutex);

    hashcode = ELFHash(url) % MAXHASH;

    if ((i = hash_lookup(hashcode, url)) < 0) {
        pthread_mutex_unlock(&cache_mutex);
        return NULL;
    }

    // find this item, update its pos.
    adjust_list_node(hash_table[i].p_list_node);
    obj = hash_table[i].p_list_node->obj;

    pthread_mutex_unlock(&cache_mutex);

    return obj;
}

int cache_insert(char *url, char *obj) {
    int hashcode, ret;

    pthread_mutex_lock(&cache_mutex);

    if (cache_items >= MAXOBJECTCOUNT)
        remove_list_node(list_node_tail);

    hashcode = ELFHash(url) % MAXHASH;
    ret = hash_insert(hashcode, url, obj);

    pthread_mutex_unlock(&cache_mutex);

    return ret;
}
