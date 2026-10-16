/* Copyright libuv contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* uv_fs_event_t on WASI (BrowserOS), from the inotify(7) of the BrowserOS
 * kernel. The libc has <sys/inotify.h> (the kernel's inotify_init1(2),
 * inotify_add_watch(2) and inotify_rm_watch(2)), and the events come from
 * read(2) on the inotify descriptor as struct inotify_event records, so this is
 * linux.c's implementation.
 */

#include "uv.h"
#include "internal.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>

struct watcher_list {
  RB_ENTRY(watcher_list) entry;
  struct uv__queue watchers;
  int iterating;
  char* path;
  int wd;
};

struct watcher_root {
  struct watcher_list* rbh_root;
};

static int compare_watchers(const struct watcher_list* a,
                            const struct watcher_list* b) {
  if (a->wd < b->wd) return -1;
  if (a->wd > b->wd) return 1;
  return 0;
}

RB_GENERATE_STATIC(watcher_root, watcher_list, entry, compare_watchers)

static struct watcher_root* uv__inotify_watchers(uv_loop_t* loop) {
  /* RB_HEAD(watcher_root, watcher_list) */
  STATIC_ASSERT(sizeof(struct watcher_root) == sizeof(loop->inotify_watchers));
  return (struct watcher_root*) &loop->inotify_watchers;
}

void uv__inotify_loop_init(uv_loop_t* loop) {
  loop->inotify_watchers = NULL;
  loop->inotify_fd = -1;
}

void uv__inotify_loop_delete(uv_loop_t* loop) {
  if (loop->inotify_fd != -1) {
    uv__io_stop(loop, &loop->inotify_read_watcher, POLLIN);
    uv__close(loop->inotify_fd);
    loop->inotify_fd = -1;
  }
}

static int init_inotify(uv_loop_t* loop) {
  int err;
  int fd;

  if (loop->inotify_fd != -1)
    return 0;

  fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (fd < 0)
    return UV__ERR(errno);

  err = uv__io_init_start(loop, &loop->inotify_read_watcher, UV__INOTIFY_READ,
                          fd, POLLIN);
  if (err) {
    uv__close(fd);
    return err;
  }

  loop->inotify_fd = fd;
  return 0;
}

static struct watcher_list* find_watcher(uv_loop_t* loop, int wd) {
  struct watcher_list w;
  w.wd = wd;
  return RB_FIND(watcher_root, uv__inotify_watchers(loop), &w);
}

static void maybe_free_watcher_list(struct watcher_list* w, uv_loop_t* loop) {
  /* if the watcher_list->watchers is being iterated over, we can't free it. */
  if ((!w->iterating) && uv__queue_empty(&w->watchers)) {
    /* No watchers left for this path. Clean up. */
    RB_REMOVE(watcher_root, uv__inotify_watchers(loop), w);
    inotify_rm_watch(loop->inotify_fd, w->wd);
    uv__free(w);
  }
}

void uv__inotify_read(uv_loop_t* loop, uv__io_t* dummy, unsigned int events) {
  const struct inotify_event* e;
  struct watcher_list* w;
  uv_fs_event_t* h;
  struct uv__queue queue;
  struct uv__queue* q;
  const char* path;
  ssize_t size;
  const char *p;
  /* needs to be large enough for sizeof(inotify_event) + strlen(path) */
  char buf[4096];

  for (;;) {
    do
      size = read(loop->inotify_fd, buf, sizeof(buf));
    while (size == -1 && errno == EINTR);

    if (size == -1) {
      assert(errno == EAGAIN || errno == EWOULDBLOCK);
      break;
    }

    if (size == 0)
      break;

    /* Now we have one or more inotify_event structs. */
    for (p = buf; p < buf + size; p += sizeof(*e) + e->len) {
      e = (const struct inotify_event*) p;

      events = 0;
      if (e->mask & (IN_ATTRIB|IN_MODIFY))
        events |= UV_CHANGE;
      if (e->mask & ~(IN_ATTRIB|IN_MODIFY))
        events |= UV_RENAME;

      w = find_watcher(loop, e->wd);
      if (w == NULL)
        continue; /* Stale event, no watchers left. */

      /* inotify does not return the filename when monitoring a single file
       * for modifications. Repurpose the filename for API compatibility.
       */
      path = e->len ? (const char*) (e + 1) : uv__basename_r(w->path);

      /* A callback can stop watchers; iterate over a moved queue and keep the
       * watcher_list until done (as linux.c does).
       */
      w->iterating = 1;
      uv__queue_move(&w->watchers, &queue);
      while (!uv__queue_empty(&queue)) {
        q = uv__queue_head(&queue);
        h = uv__queue_data(q, uv_fs_event_t, watchers);

        uv__queue_remove(q);
        uv__queue_insert_tail(&w->watchers, q);

        h->cb(h, path, events, 0);
      }
      w->iterating = 0;
      maybe_free_watcher_list(w, loop);
    }
  }
}

int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle) {
  uv__handle_init(loop, (uv_handle_t*)handle, UV_FS_EVENT);
  return 0;
}

int uv_fs_event_start(uv_fs_event_t* handle,
                      uv_fs_event_cb cb,
                      const char* path,
                      unsigned int flags) {
  struct watcher_list* w;
  uv_loop_t* loop;
  size_t len;
  int events;
  int err;
  int wd;

  if (uv__is_active(handle))
    return UV_EINVAL;

  loop = handle->loop;

  err = init_inotify(loop);
  if (err)
    return err;

  events = IN_ATTRIB
         | IN_CREATE
         | IN_MODIFY
         | IN_DELETE
         | IN_DELETE_SELF
         | IN_MOVE_SELF
         | IN_MOVED_FROM
         | IN_MOVED_TO;

  wd = inotify_add_watch(loop->inotify_fd, path, events);
  if (wd == -1)
    return UV__ERR(errno);

  w = find_watcher(loop, wd);
  if (w)
    goto no_insert;

  len = strlen(path) + 1;
  w = uv__malloc(sizeof(*w) + len);
  if (w == NULL)
    return UV_ENOMEM;

  w->wd = wd;
  w->path = memcpy(w + 1, path, len);
  uv__queue_init(&w->watchers);
  w->iterating = 0;
  RB_INSERT(watcher_root, uv__inotify_watchers(loop), w);

no_insert:
  uv__handle_start(handle);
  uv__queue_insert_tail(&w->watchers, &handle->watchers);
  handle->path = w->path;
  handle->cb = cb;
  handle->wd = wd;

  return 0;
}

int uv_fs_event_stop(uv_fs_event_t* handle) {
  struct watcher_list* w;

  if (!uv__is_active(handle))
    return 0;

  w = find_watcher(handle->loop, handle->wd);
  assert(w != NULL);

  handle->wd = -1;
  handle->path = NULL;
  uv__handle_stop(handle);
  uv__queue_remove(&handle->watchers);

  maybe_free_watcher_list(w, handle->loop);

  return 0;
}

void uv__fs_event_close(uv_fs_event_t* handle) {
  uv_fs_event_stop(handle);
}
