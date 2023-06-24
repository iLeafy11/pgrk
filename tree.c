#define _GNU_SOURCE
/* Word cache configs */
#define MAX_WORD_SIZE 32
#define MAX_N 8192

#include "ht.h"

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
static struct g_cache cache;

static void g_cache_init()
{
    cache.i = 0;
}

static void g_cache_add(struct list_head *node)
{
    cache.entry[cache.i] = node;
    cache.i++;
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
    struct ht_cache *cache = &click_cache; /*FIXME: */
    struct ht_node *n;
    if (!(n = ht_find(&cache->htable, name))) {
        /* word was absent. Allocate a new wc_word */
        if (!(g = calloc(1, sizeof(struct g_vertex)))) return NULL;

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
    if (!dir) return;

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
                g_cache_add(&new->worklist);
                entry->out[entry->outDegree].to = new;
                entry->outDegree++;
                new->in[new->inDegree] = entry;
                new->inDegree++;
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

struct g_vertex *g;

int main(int argc, char *argv[]) {

    w_init(64);
    char *dir = strdup(argv[argc - 1]);

    g = g_add_name(dir, strlen(dir));
    g_cache_add(&g->worklist);
    // strcpy(g->name, argv[argc - 1]);
    
    tree(g, dir, dir, 0);
    w_print();
    /*
    printf("\n===\n");
    print_graph(g);
    printf("\n");
    w_print();
    */
    return 0;
}

