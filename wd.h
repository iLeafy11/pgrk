#ifndef _HT_H
#define _HT_H
#define _GNU_SOURCE
#define _XOPEN_SOURCE 500
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <string.h>

#include "list.h"
typedef struct watched_dirs {
    int wd;
    char name[256];
    struct list_head list;
} W_DIRS;

static int wd_add(int fd, struct list_head *list, const char *path);

static int wd_delete(int fd, struct list_head *list, char *path);

static int wd_destroy(struct list_head *list);

static int wd_print_list(struct list_head *list);

static W_DIRS *wd_find(const struct inotify_event *e, struct list_head *list);

static void wd_find_path(char *buf, const struct inotify_event *e, struct list_head *list);

static void wd_handle_events(int fd, struct list_head *list);

#include <ftw.h>
#include <string.h>
#include <stdint.h>

static int wd_watch(const char *fpath, const struct stat *sb,
                  int tflag, struct FTW *ftwbuf);

#include <sys/epoll.h>

static int epoll_add(int epoll_fd, int file_fd);


static int epoll_query_init(int *epoll_fd, int argc, char *argv[]);

#endif
