#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include "wd.h"

int inotify_fd;
struct list_head wds_list;

static inline int wd_add(int fd, struct list_head *list, const char *path)
{
    W_DIRS *new = malloc(sizeof(W_DIRS));
    strcpy(new->name, path);
    new->wd = inotify_add_watch(fd, new->name, IN_ALL_EVENTS);
    list_add(&new->list, list);
    return 0;
}

static int wd_delete(int fd, struct list_head *list, char *path)
{
    struct list_head *ptr, *node;
    list_for_each_safe(ptr, node, list) {
        W_DIRS *wd = list_entry(ptr, W_DIRS, list);
        if (strcmp(wd->name, path) == 0) {
            printf("target:%s, %s\n", path, wd->name);
            list_remove(ptr);
            return 0;
        }
        free(wd);
    }
}

static int wd_destroy(struct list_head *list)
{
    struct list_head *ptr, *node;
    list_for_each_safe(ptr, node, list) {
        W_DIRS *wd = list_entry(ptr, W_DIRS, list);
        free(wd);
    }
}

static int wd_print_list(struct list_head *list) {
    struct list_head *entry;
    list_for_each(entry, list) {
        W_DIRS *tmp = list_entry(entry, W_DIRS, list);
        printf("list: %s/\n", tmp->name);
    }

    return 0;
}

static W_DIRS *wd_find(const struct inotify_event *e, struct list_head *list)
{
    W_DIRS *wd;
    struct list_head *entry;
    list_for_each(entry, list) {
        wd = list_entry(entry, W_DIRS, list);
        if (wd->wd == e->wd) break;
    }

    return wd;
}

static inline void wd_find_path(char *buf, const struct inotify_event *e, struct list_head *list)
{
    W_DIRS *wd = wd_find(e, list);
    strcpy(buf, wd->name); /* strcpy unsafe function */
    strcat(buf, "/");
    strcat(buf, e->name);
    strcat(buf, "\0");
}

static void wd_handle_events(int fd, struct list_head *list)
{
    char buf[4096]
    __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
    ssize_t len;

    /* Loop while events can be read from inotify file descriptor. */

    for (;;) {

        /* Read some events. */

        len = read(fd, buf, sizeof(buf));
        if (len == -1 && errno != EAGAIN) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        /* If the nonblocking read() found no events to read, then
           it returns -1 with errno set to EAGAIN. In that case,
           we exit the loop. */

        if (len <= 0)
            break;

        /* Loop over all events in the buffer. */

        for (char *ptr = buf; ptr < buf + len;
             ptr += sizeof(struct inotify_event) + event->len) {

            event = (const struct inotify_event *) ptr;

            /* Print event type. */
            /* IS_DIR */

            if (event->mask & IN_CREATE) {
                printf("IN_CREATE: ");
                char path_buf[4096];
                wd_find_path(path_buf, event, list);
                wd_add(fd, list, path_buf);
            }

            if (event->mask & IN_DELETE) {
                char path_buf[4096];
                wd_find_path(path_buf, event, list);
                printf("IN_DELETE: %s\n", path_buf);
                wd_delete(fd, list, path_buf);
            }


            if (event->mask & IN_ACCESS) {
                printf("IN_ACCESS: ");
                /* do something */
            }

            if (event->mask & IN_OPEN)
                printf("IN_OPEN: ");
            if (event->mask & IN_CLOSE_NOWRITE)
                printf("IN_CLOSE_NOWRITE: ");
            if (event->mask & IN_CLOSE_WRITE)
                printf("IN_CLOSE_WRITE: ");

            /* Print the name of the watched directory. */
            
            struct list_head *entry;
            list_for_each(entry, list) {
                W_DIRS *tmp = list_entry(entry, W_DIRS, list);
                if (tmp->wd == event->wd) {
                    printf("%s/", tmp->name);
                    break;
                }
            }
            
            /* Print the name of the file. */
            
            if (event->len)
                printf("%s", event->name);
            
            /* Print type of filesystem object. */

            if (event->mask & IN_ISDIR)
                printf(" [directory]\n");
            else
                printf(" [file]\n");
        }
    }
}

#include <ftw.h>
#include <string.h>
#include <stdint.h>

#include "tree.h"

static int wd_watch(const char *fpath, const struct stat *sb,
                  int tflag, struct FTW *ftwbuf)
{
    
    // printf("fpath:%s\n", fpath + ftwbuf->base);
    if ((tflag == FTW_D) || (tflag == FTW_DNR) || (tflag == FTW_DP)) {
        wd_add(inotify_fd, &wds_list, fpath);

    }
    return 0;
}

#include <sys/epoll.h>

static int epoll_add(int epoll_fd, int file_fd)
{
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = file_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, file_fd, &ev)) {
        perror("epoll_ctr add file_fd failed.");
        return 1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &ev)) {
        perror("epoll_ctr add stdin failed.");
        return 1;
    }

    return 0;
}

static int epoll_query_init(int *epoll_fd, int argc, char *argv[])
{
    *epoll_fd = epoll_create1(0);
    if (*epoll_fd == -1) {
        perror("Epoll create error\n");
        exit(EXIT_FAILURE);
    }

    if (argc < 2) {
        printf("Usage: %s PATH [PATH ...]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Press ENTER key to terminate.\n");

    /* Create the file descriptor for accessing the inotify API. */

    inotify_fd = inotify_init1(IN_NONBLOCK);
    if (inotify_fd == -1) {
        perror("inotify_init1");
        exit(EXIT_FAILURE);
    }

    /* Allocate memory for watch descriptors. */

    INIT_LIST_HEAD(&wds_list);

    /* Mark directories for events
       - file was opened
       - file was closed */

    int flags = 0;
    for (int i = 1; i < argc; i++) {
        // wd_add(fd, &wds_list, argv[i]);
        if (nftw((argc < 2) ? "." : argv[1], wd_watch, 20, flags) == -1) {
            perror("nftw");
            exit(EXIT_FAILURE);
        }
    }

    /* Prepare for polling. */

    epoll_add(*epoll_fd, inotify_fd);

    return 0;
}

struct g_vertex *g;

#define for_each_in_neighbor(inlinks, vertex)	    \
	for ((inlinks) = vertex->in[0];       \
	     (inlinks) < vertex->in[vertex->inDegree]; \
	     (inlinks)++)

#define for_each_out_neighbor(outlinks, vertex)	    \
	for ((outlinks) = vertex->out[0];       \
	     (outlinks) < vertex->out[vertex->outDegree]; \
	     (outlinks)++)

static int first_iter(double alpha, struct list_head *worklist)
{
    struct list_head *it;
    struct g_vertex *v, *w;
    list_for_each(it, worklist) {
        v = list_entry(it, struct g_vertex, worklist);
        for (int i = 0; i < v->inDegree; i++) {
            w = v->in[i];
            v->r += (double) 1 / (double) w->outDegree;
        }
        /*
        for_each_in_neighbor(w, v) {
            v->r += (double) 1 / (double) w->outDegree;
        }        
        */
        v->r = (1 - alpha) * alpha * v->r;
        // printf("first: %s: %lf\n", GET_NAME(v), v->r);
    }
    return 0;
}

static int pgrk_one_vertex(struct g_vertex *v, double alpha, double epsilon, 
        struct list_head *local)
{
    if (list_empty(local)) {
        return -1;
    }
    double r_old;
    struct g_vertex *w;
    for (int i = 0; i < v->outDegree; i++) {
        w = v->out[i].to;
        r_old = w->r;
        
        w->r += alpha * v->r / (double) v->outDegree; /* unweighted */
        
        printf("%s: %lf, %lf\n", GET_NAME(w), w->r, v->r);
        if (w->r >= epsilon && r_old < epsilon) {
            // printf("%s: %lf\n", GET_NAME(v), v->r);
            list_add(&w->worklist, local);
            struct list_head *iter;
            list_for_each(iter, local) {
                struct g_vertex *tmp = list_entry(iter, struct g_vertex, worklist);
                // printf("%s\n", GET_NAME(v));
            }
        }
        
    }
    /*
    for_each_out_neighbor(w, v) {
        r_old = w->r;
        w->r += alpha * v->r / (double) v->outDegree;
        if (w->r >= epsilon && r_old < epsilon)
            list_add(&w->worklist, &thread_worklists[tid]);
    }
    */
}

static inline struct list_head *list_pop(struct list_head *head)
{
    if (list_empty(head)) return NULL;

    struct list_head *tail;
    tail = head->prev;
    if (tail) {
        list_remove(tail);
    }
    return tail;
}

static int pgrk_push_based (double alpha, double epsilon, struct list_head *local)
{
    //printf("%s: nothing\n", GET_NAME(v));

    struct list_head *curr;
    while (curr = list_pop(local)) {
        struct g_vertex *v = list_entry(curr, struct g_vertex, worklist);
        v->p += v->r;
        pgrk_one_vertex(v, alpha, epsilon, local);
    }
}

static int pgrk(uint32_t n_threads, uint32_t tid)
{
    uint32_t g_slice = main_cache.i / n_threads;
    uint32_t g_current, g_end;
    g_current = g_slice * tid;
    
    /* Last thread handle remaining vertices */
    if (tid == (n_threads - 1)) g_slice += main_cache.i % n_threads;

    g_end = g_current + g_slice;
    LIST_HEAD(local);
    for (int i = 0; i < main_cache.i; i++) {
        list_add(main_cache.entry[i], &local);
    }

    first_iter(0.85, &local);
    pgrk_push_based(0.85, 0.0001, &local);
    
    for (int i = g_current; i < g_end; i++) {
        struct g_vertex *tmp = list_entry(main_cache.entry[i], struct g_vertex, worklist);
        //printf("%s: %lf\n", GET_NAME(tmp), tmp->r);
    }

    /*
    struct list_head *iter;
    
    list_for_each(iter, &local) {
        struct g_vertex *tmp = list_entry(iter, struct g_vertex, worklist);
        printf("%s\n", GET_NAME(tmp));
        //printf("%lf\n", tmp->r);
    }
    */

    /*
    for (int i = graph_current; i < graph_end; i++) {
        value = 0.0;
        // sum and 0.85 factor
        list_add(&v[i].list, list); // push
        // pgrk_pb();
    }
     */
}


int main(int argc, char *argv[])
{
    int epoll_fd;
    epoll_query_init(&epoll_fd, argc, argv);
    
    w_init(64);
    
    char *dir = strdup(argv[argc - 1]);
    
    g = g_add_name(dir, strlen(dir));
    g_cache_add(&g->worklist);
    
    tree(g, dir, dir, 0);
    pgrk(4, 3);
    /*
    for (int i = 0; i < main_cache.i; i++) {
        struct g_vertex *tmp = list_entry(main_cache.entry[i], struct g_vertex, worklist);\
        for (int j = 0; j < tmp->outDegree; j++) {
            printf("%s: %d\n", GET_NAME(tmp->out[j].to), tmp->out[j].click);
        }
    }
    */
    // w_print();
    /* Wait for events and/or terminal input. */

    printf("Listening for events.\n");
    int nfds, tmpfd;
    char buf;
    struct epoll_event *events =
        (struct epoll_event *) malloc(sizeof(struct epoll_event) * 64);
    struct epoll_event event;

    while (1) {
        nfds = epoll_wait(epoll_fd, events, 64, -1);
        
        if (nfds == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; i++) {
            event = events[i];
            tmpfd = event.data.fd;
            if (tmpfd == inotify_fd) {
                wd_handle_events(inotify_fd, &wds_list);
                continue;
            }

            if (tmpfd == STDIN_FILENO) {
                while (read(STDIN_FILENO, &buf, 1) > 0 && buf != '\n')
                    continue;
                goto bail;
            }
        }
    }

bail:
    printf("Listening for events stopped.\n");

    /* Close inotify file descriptor. */

    close(inotify_fd);
    free(events);
    wd_destroy(&wds_list);
    exit(EXIT_SUCCESS);
}
