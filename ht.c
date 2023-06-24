#include "ht.h"

inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

inline bool hlist_is_singular_node(struct hlist_node *n,
                                          struct hlist_head *h)
{
    return !n->next && n->pprev == &h->first;
}

inline struct hlist_node *hlist_next(struct hlist_head *root,
                                            struct hlist_node *current)
{
    if ((hlist_empty(root)) || hlist_is_singular_node(current, root) ||
        !current)
        return NULL;
    return current->next;
}


inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->next = NULL;
    h->pprev = NULL;
}

inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

void __hash_init(struct hlist_head *ht, unsigned int sz)
{
    for (unsigned int i = 0; i < sz; i++)
        INIT_HLIST_HEAD(&ht[i]);
}

/* Initializes a hash table */
int ht_init(struct htable *h,
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
inline int ht_destroy(struct htable *h)
{
    free(h->buckets);
    return 0;
}

/* Find an element with the key in the hash table.
 * Return the element if success.
 */
struct ht_node *ht_find(struct htable *h, void *key)
{
    hash_t hval;
    uint32_t bkt;
    h->hashfunc(key, &hval, &bkt);

    struct hlist_head *head = &h->buckets[bkt];
    struct ht_node *n;
    hlist_for_each_entry (n, head, hlist) {
        if (n->hash == hval) {
            int res = h->cmp(n, key);
            if (!res) return n;
            /* if (res < 0) return NULL; */
        }
    }
    return NULL;
}

inline void hlist_add_behind(struct hlist_node *n,
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
int ht_insert(struct htable *h, struct ht_node *n, void *key)
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
                hlist_add_behind(&n->hlist, iter);
                return 0;
            }
        }
    }
    
    h->size++;
    hlist_add_head(&n->hlist, head);
    return 0;
}

struct ht_node *ht_get_first(struct htable *h, uint32_t bucket)
{
    struct hlist_head *head = &h->buckets[bucket];
    if (hlist_empty(head)) return NULL;
    return hlist_first_entry(head, struct ht_node, hlist);
}

struct ht_node *ht_get_next(struct htable *h,
                                          uint32_t bucket,
                                          struct ht_node *n)
{
    struct hlist_node *ln = hlist_next(&h->buckets[bucket], &n->hlist);
    if (!ln) return NULL;
    return hlist_entry(ln, struct ht_node, hlist);
}

