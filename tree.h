#ifndef _TREE_H
#define _TREE_H
#define _GNU_SOURCE
/* Word cache configs */
#define MAX_WORD_SIZE 32
#define MAX_N 8192

#include <stddef.h>
#include "list.h"
/*
#define container_of(list_ptr, container_type, member_name)               \
    ({                                                                    \
        const typeof(((container_type *) 0)->member_name) *__member_ptr = \
            (list_ptr);                                                   \
        (container_type *) ((char *) __member_ptr -                       \
                            offsetof(container_type, member_name));       \
    })
*/
#define hlist_entry(ptr, type, member) container_of(ptr, type, member)

#define hlist_first_entry(head, type, member) \
    hlist_entry((head)->first, type, member)

struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next, **pprev;
};

#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

#include <stdbool.h>

static inline bool hlist_is_singular_node(struct hlist_node *n,
                                          struct hlist_head *h)
{
    return !n->next && n->pprev == &h->first;
}

static inline struct hlist_node *hlist_next(struct hlist_head *root,
                                            struct hlist_node *current)
{
    if ((hlist_empty(root)) || hlist_is_singular_node(current, root) ||
        !current)
        return NULL;
    return current->next;
}

#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos; pos = pos->next)

#define hlist_entry_safe(ptr, type, member)                  \
    ({                                                       \
        typeof(ptr) ____ptr = (ptr);                         \
        ____ptr ? hlist_entry(____ptr, type, member) : NULL; \
    })

#define hlist_for_each_entry(pos, head, member)                              \
    for (pos = hlist_entry_safe((head)->first, typeof(*(pos)), member); pos; \
         pos = hlist_entry_safe((pos)->member.next, typeof(*(pos)), member))

static inline void __hash_init(struct hlist_head *ht, unsigned int sz)
{
    for (unsigned int i = 0; i < sz; i++)
        INIT_HLIST_HEAD(&ht[i]);
}


static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->next = NULL;
    h->pprev = NULL;
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

#include <stdint.h>
#include <stdlib.h>

#include "list.h"

typedef uint32_t hash_t;

/* A node of the table */
struct ht_node {
    hash_t hash;
    struct hlist_node hlist;
};

/* user-defined functions */
typedef int hashfunc_t(void *key, hash_t *hash, uint32_t *bkt);
typedef int cmp_t(struct ht_node *n, void *key);

/* hash table */
struct htable {
    hashfunc_t *hashfunc;
    cmp_t *cmp;
    uint32_t n_buckets;
    uint32_t size;
    struct hlist_head *buckets;
};

/* Initializes a hash table */
static inline int ht_init(struct htable *h,
                          hashfunc_t *hashfunc,
                          cmp_t *cmp,
                          uint32_t n_buckets)
{
    h->hashfunc = hashfunc, h->cmp = cmp;
    h->n_buckets = n_buckets;
    h->size = 0;
    h->buckets = malloc(sizeof(struct hlist_head) * n_buckets);
    __hash_init(h->buckets, h->n_buckets);
    return 0;
}

/* destroys ressource called by ht_init */
static inline int ht_destroy(struct htable *h)
{
    free(h->buckets);
    return 0;
}

/* Find an element with the key in the hash table.
 * Return the element if success.
 */
#include <stdio.h>

static inline struct ht_node *ht_find(struct htable *h, void *key)
{
    hash_t hval;
    uint32_t bkt;
    h->hashfunc(key, &hval, &bkt);

    struct hlist_head *head = &h->buckets[bkt];
    struct ht_node *n;
    /*
    //printf("%s\n", (char *)key);
    hlist_for_each_entry (n, head, list) {
        int cmp = h->cmp(n, "a");
        printf("a: [%u] %d, key: %s\n", bkt, cmp, (char *)key);
        cmp = h->cmp(n, "aa");
        printf("aa: [%u] %d, key: %s\n", bkt, cmp, (char *)key);
        cmp = h->cmp(n, "aaa");
        printf("aaa: [%u] %d, key: %s\n", bkt, cmp, (char *)key);
    }
    printf("===\n");
    */
    hlist_for_each_entry (n, head, hlist) {
        if (n->hash == hval) {
            int res = h->cmp(n, key);
            if (!res) return n;
            /* if (res < 0) return NULL; */
        }
    }
    return NULL;
}

static inline void hlist_add_behind(struct hlist_node *n,
				    struct hlist_node *prev)
{
	n->next = prev->next;
	prev->next = n;
	n->pprev = &prev->next;

	if (n->next)
		n->next->pprev = &n->next;
}

/* Insert a new element with the key 'key' in the htable.
 * Return 0 if success.
 */
static inline int ht_insert(struct htable *h, struct ht_node *n, void *key)
{
    hash_t hval;
    uint32_t bkt;
    h->hashfunc(key, &hval, &bkt);
    n->hash = hval;
    
    struct hlist_head *head = &h->buckets[bkt];
    struct hlist_node *iter;
    hlist_for_each (iter, head) {
        struct ht_node *tmp = hlist_entry(iter, struct ht_node, hlist);
        if (tmp->hash >= hval) {

            int cmp = h->cmp(tmp, key);
            if (!cmp) /* already exist */{
                return -1;

            }
            if (cmp < 0) {
                h->size++;
                // printf("[%d] insert: %s, %d\n", bkt, (char *)key, h->n_buckets);
                hlist_add_behind(&n->hlist, iter);
                return 0;
            }
        }
    }
    
    h->size++;
    hlist_add_head(&n->hlist, head);
    return 0;
}

static inline struct ht_node *ht_get_first(struct htable *h, uint32_t bucket)
{
    struct hlist_head *head = &h->buckets[bucket];
    if (hlist_empty(head)) return NULL;
    return hlist_first_entry(head, struct ht_node, hlist);
}

static inline struct ht_node *ht_get_next(struct htable *h,
                                          uint32_t bucket,
                                          struct ht_node *n)
{
    struct hlist_node *ln = hlist_next(&h->buckets[bucket], &n->hlist);
    if (!ln) return NULL;
    return hlist_entry(ln, struct ht_node, hlist);
}

/* cache of words. Count the number of word using a modified hash table */

struct g_cache {
    uint32_t i;
    struct list_head *entry[256];
};

struct ht_cache {
    struct htable htable;
};

struct g_vertex;

struct g_edge {
    struct g_vertex *to;
    uint32_t click;
};

struct g_vertex {
    double r, p;
    char name[MAX_WORD_SIZE], *full_name;
    struct g_vertex *in[20]; /* a resizable array would be better */
    // struct g_vertex *out[20];
    struct g_edge out[20];
    uint32_t inDegree;
    uint32_t outDegree;
    struct list_head worklist;
    struct ht_node node, node_main;
};

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* TODO: handle '-' character (hyphen) */
/* TODO: add number support */
/* FIXME: remove the assumptions on ASCII encoding */

static uint32_t n_buckets;
static struct ht_cache click_cache;
static struct g_cache main_cache;

static void g_cache_init()
{
    main_cache.i = 0;
}

static void g_cache_add(struct list_head *node)
{
    main_cache.entry[main_cache.i] = node;
    main_cache.i++;
}

#define FIRST_LOWER_LETTER 'a'
#define N_LETTERS (('z' - 'a') + 1)
#define MIN_N_BUCKETS N_LETTERS

#define GET_NAME(w) (w->full_name ? w->full_name : w->name)

#define MIN_MAX(a, b, op)       \
    ({                          \
        __typeof__(a) _a = (a); \
        __typeof__(b) _b = (b); \
        _a op _b ? _a : _b;     \
    })

#define MAX(a, b) MIN_MAX(a, b, >)
#define MIN(a, b) MIN_MAX(a, b, <)

/* lowerbound (floor log2) */
static inline uint32_t ilog2(uint32_t n)
{
    return 32 - __builtin_clz(n) - 1;
}

#include <strings.h>
/* Called to compare word if their hash value is similar */
static inline int __cmp(struct ht_node *n, void *key, char m)
{
    struct g_vertex *w = m ? container_of(n, struct g_vertex, node_main)
                          : container_of(n, struct g_vertex, node);
    return strcasecmp(GET_NAME(w), (char *) key);
}

static int cmp(struct ht_node *n, void *key)
{
    return __cmp(n, key, 0);
}

static int cmp_main(struct ht_node *n, void *key)
{
    return __cmp(n, key, 1);
}

static uint32_t code_min, code_max, code_range;

static uint32_t get_code(char *word)
{
    uint32_t code = 0;
    /* The hash value is a concatenation of the letters */
    char shift =
        (char) (sizeof(unsigned int) * CHAR_BIT - __builtin_clz(N_LETTERS));

    for (int i = ((sizeof(code) * 8) / shift) - 1; i >= 0 && *word; i--) {
        uint32_t t = isdigit(*word) ? (uint32_t) (*(word) - '0')
            : (uint32_t) (tolower(*(word)) - FIRST_LOWER_LETTER);
        code |= t << (i * shift);
        word++;
    }
    return code;
}

static inline int scale_range_init()
{
    code_min = get_code("a"), code_max = get_code("zzzzzzzzzz");
    code_range = (code_max - code_min);
    return 0;
}

static inline uint32_t scale_range(uint32_t code)
{
    return (uint32_t)((((double) code - code_min) * n_buckets) / code_range) % n_buckets;
    /* This hash is terrible */
}

/* Must keep an an alphabetic order when assiging buckets. */
static int hash_bucket(void *key, hash_t *hash, uint32_t *bkt)
{
    uint32_t code = get_code((char *) key);
    *hash = (hash_t) code, *bkt = scale_range(code);
    return 0;
}

/* Initialize each (worker+main) cache */
int w_init(uint32_t n_vertices)
{
    /* FIXME: a resizable hash table would be better */
    n_buckets = MAX(MIN(n_vertices, MAX_N), MIN_N_BUCKETS);
    scale_range_init();
    
    if (ht_init(&click_cache.htable, hash_bucket, cmp, n_buckets))
        return -1;
    
    return 0;
}

/* Copy while setting to lower case */
static char *w_strncpy(char *dest, char *src, size_t n)
{
    /* case insensitive */
    for (size_t i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = (char) tolower((int) (src[i]));
    return dest;
}

#define GET_NAME(w) (w->full_name ? w->full_name : w->name)

struct g_vertex *g_add_name(char *name, uint32_t count)
{
    struct g_vertex *g;
    struct ht_cache *cache = &click_cache;
    struct ht_node *n;
    if (!(n = ht_find(&cache->htable, name))) {
        /* word was absent. Allocate a new wc_word */
        if (!(g = calloc(1, sizeof(struct g_vertex)))) return NULL;
        g->inDegree = 0;
        g->outDegree = 0;
        g->r = 0.0f;
        g->p = 0.0f;
        if (count > (MAX_WORD_SIZE - 1)) g->full_name = calloc(1, count + 1);

        w_strncpy(GET_NAME(g), name, count);

        /* Add the new world to the table */
        // printf("name: %s\n", name);
        ht_insert(&cache->htable, &g->node, name);
    } else 
        g = container_of(n, struct g_vertex, node); /* This else is redundant */
    
    return g;
}


#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

static __thread char *worker_buffer;

#if defined(__linux__)
#define MMAP_FLAGS (MAP_POPULATE | MAP_PRIVATE)
#else
#define MMAP_FLAGS (MAP_PRIVATE)
#endif

#include <sys/types.h>
#include <dirent.h>

void tree(struct g_vertex *entry, char *name, char *base, int root) {
    DIR *dir = opendir(base);
    if (!dir) 
        return;

    char path[256];
    struct dirent *dp;
    while ((dp = readdir(dir)))
    {
        if (strcmp(dp->d_name, ".") && strcmp(dp->d_name, "..") && (dp->d_name[0] != '.')) {

            strcpy(path, base);
            strcat(path, "/");
            strcat(path, dp->d_name);
            
            if(dp->d_type != DT_REG) {
                struct g_vertex *new = g_add_name(dp->d_name, strlen(dp->d_name));
                entry->out[entry->outDegree].to = new;
                entry->out[entry->outDegree].click = 1;
                entry->outDegree++;
                new->in[new->inDegree] = entry;
                new->inDegree++;
                g_cache_add(&new->worklist);
                tree(new, dp->d_name, path, root + 1);
            }
        }
    }

    closedir(dir);
}

#define for_each_in_neighbor(inlinks, vertex)	    \
	for ((inlinks) = vertex->in[0];       \
	     (inlinks) < vertex->in[vertex->inDegree]; \
	     (inlinks)++)

#define for_each_out_neighbor(outlinks, vertex)	    \
	for ((outlinks) = vertex->out[0];       \
	     (outlinks) < vertex->out[vertex->outDegree]; \
	     (outlinks)++)

int w_print()
{
    struct ht_cache *cache = &click_cache;

    for (size_t j = 0; j < n_buckets; j++) {
        struct ht_node *iter = ht_get_first(&cache->htable, j);
        struct hlist_node *i = &iter->hlist;
        /*
        hlist_for_each(i, &cache->htable.buckets[j]) {
            struct ht_node *tmp = hlist_entry(i, struct ht_node, hlist);
            struct g_vertex *g0 = container_of(tmp, struct g_vertex, node);
            printf("[%lu], %s : %d\n", j, GET_NAME(g0), g0->outDegree);
        }
        */
        for (; iter; iter = ht_get_next(&cache->htable, j, iter)) {
            struct g_vertex *g = container_of(iter, struct g_vertex, node);
            printf("[%lu], %s\tout: %d\tin: %d\n", j, GET_NAME(g), g->outDegree, g->inDegree);
        }
        
    }
    /*
    printf("Words: %d, word counts: %d, full buckets: %d (ideal %d)\n", total,
           count_total, n_buckets - empty_bkt, MIN(total, n_buckets));
           */
    return 0;
}

void print_graph(struct g_vertex *g)
{
    printf("%s\n", g->name);
    for (int i = 0; i < g->outDegree; i++) {
        print_graph(g->out[i].to);
    }
}


#endif

