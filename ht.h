#ifndef _HT_H
#define _HT_H

#define _GNU_SOURCE
/* Word cache configs */
#define MAX_WORD_SIZE 32
#define MAX_N 8192

#include <stddef.h>

#define container_of(list_ptr, container_type, member_name)               \
    ({                                                                    \
        const typeof(((container_type *) 0)->member_name) *__member_ptr = \
            (list_ptr);                                                   \
        (container_type *) ((char *) __member_ptr -                       \
                            offsetof(container_type, member_name));       \
    })

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

static inline int hlist_empty(const struct hlist_head *h);

#include <stdbool.h>

static inline bool hlist_is_singular_node(struct hlist_node *n,
                                          struct hlist_head *h);

static inline struct hlist_node *hlist_next(struct hlist_head *root,
                                            struct hlist_node *current);

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


static inline void INIT_HLIST_NODE(struct hlist_node *h);

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h);

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

void __hash_init(struct hlist_head *ht, unsigned int sz);
/* Initializes a hash table */
int ht_init(struct htable *h,
                          hashfunc_t *hashfunc,
                          cmp_t *cmp,
                          uint32_t n_buckets);

/* destroys ressource called by ht_init */
static inline int ht_destroy(struct htable *h);

/* Find an element with the key in the hash table.
 * Return the element if success.
 */
#include <stdio.h>

struct ht_node *ht_find(struct htable *h, void *key);

static inline void hlist_add_behind(struct hlist_node *n,
				    struct hlist_node *prev);

/* Insert a new element with the key 'key' in the htable.
 * Return 0 if success.
 */
int ht_insert(struct htable *h, struct ht_node *n, void *key);

struct ht_node *ht_get_first(struct htable *h, uint32_t bucket);

struct ht_node *ht_get_next(struct htable *h,
                                          uint32_t bucket,
                                          struct ht_node *n);
#endif
