/* Copyright libuv project contributors. All rights reserved.
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

#include "uv.h"
#include "internal.h"

/* POSIX defines poll() as a portable way to wait on file descriptors.
 * Here we maintain a dynamically sized array of file descriptors and
 * events to pass as the first argument to poll().
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

int uv__platform_loop_init(uv_loop_t* loop) {
  loop->poll_fds = NULL;
  loop->poll_fds_used = 0;
  loop->poll_fds_size = 0;
  loop->poll_fds_iterating = 0;
#if defined(__wasi__)
  uv__inotify_loop_init(loop);
#endif
  return 0;
}

void uv__platform_loop_delete(uv_loop_t* loop) {
#if defined(__wasi__)
  uv__inotify_loop_delete(loop);
#endif
  uv__free(loop->poll_fds);
  loop->poll_fds = NULL;
}

int uv__io_fork(uv_loop_t* loop) {
  uv__platform_loop_delete(loop);
  return uv__platform_loop_init(loop);
}

#if defined(__wasi__)
#include <wasi/api.h>

/* BrowserOS: a thread whose host engine runs JavaScript (node's isolates in
 * @progate/browser-v8) has work of the host's own to do while the loop waits:
 * promises the host settles (WebAssembly.compile, Atomics.waitAsync), objects
 * other threads pass to it. Blocking in poll() would keep the host's event
 * loop from turning, so such a thread waits in a poll_oneoff that suspends it
 * (JSPI) instead. Whether it can is the embedder's to say: only when no
 * JavaScript is on the stack. */
__attribute__((import_module("browser_wasi_async"), import_name("poll_oneoff")))
int32_t uv__wasi_poll_oneoff_suspending(const __wasi_subscription_t* in,
                                        __wasi_event_t* out,
                                        uint32_t nsubscriptions,
                                        uint32_t* nevents);

int uv__wasi_can_suspend(void) __attribute__((weak));

/* poll_oneoff reports the errno of WASI; the libc's errno is Linux's. */
static int uv__wasi_errno(int32_t error) {
  switch (error) {
    case __WASI_ERRNO_2BIG: return E2BIG;
    case __WASI_ERRNO_ACCES: return EACCES;
    case __WASI_ERRNO_AGAIN: return EAGAIN;
    case __WASI_ERRNO_BADF: return EBADF;
    case __WASI_ERRNO_FAULT: return EFAULT;
    case __WASI_ERRNO_INTR: return EINTR;
    case __WASI_ERRNO_INVAL: return EINVAL;
    case __WASI_ERRNO_IO: return EIO;
    case __WASI_ERRNO_NOMEM: return ENOMEM;
    case __WASI_ERRNO_NOSYS: return ENOSYS;
    case __WASI_ERRNO_NOTSUP: return ENOTSUP;
    case __WASI_ERRNO_PIPE: return EPIPE;
    default: return EIO;
  }
}

/* poll(2) through the suspending poll_oneoff, as wasi-libc builds it. */
static int uv__wasi_poll_suspending(struct pollfd* fds,
                                    nfds_t nfds,
                                    int timeout) {
  __wasi_subscription_t subscriptions[2 * nfds + 1];
  __wasi_event_t events[2 * nfds + 1];
  uint32_t nsubscriptions = 0;
  uint32_t nevents = 0;
  nfds_t i;
  int count;

  for (i = 0; i < nfds; i++) {
    fds[i].revents = 0;
    if (fds[i].fd < 0) continue;
    if (fds[i].events & (POLLIN | POLLRDNORM)) {
      subscriptions[nsubscriptions++] = (__wasi_subscription_t){
          .userdata = (uintptr_t)&fds[i],
          .u.tag = __WASI_EVENTTYPE_FD_READ,
          .u.u.fd_read.file_descriptor = fds[i].fd,
      };
    }
    if (fds[i].events & (POLLOUT | POLLWRNORM)) {
      subscriptions[nsubscriptions++] = (__wasi_subscription_t){
          .userdata = (uintptr_t)&fds[i],
          .u.tag = __WASI_EVENTTYPE_FD_WRITE,
          .u.u.fd_write.file_descriptor = fds[i].fd,
      };
    }
  }
  if (timeout >= 0 || nsubscriptions == 0) {
    uint64_t ns = timeout >= 0 ? (uint64_t)timeout * 1000000 : UINT64_C(0x7fffffff) * 1000000000;
    subscriptions[nsubscriptions++] = (__wasi_subscription_t){
        .u.tag = __WASI_EVENTTYPE_CLOCK,
        .u.u.clock.id = __WASI_CLOCKID_MONOTONIC,
        .u.u.clock.timeout = ns,
    };
  }

  int32_t error = uv__wasi_poll_oneoff_suspending(subscriptions, events,
                                                  nsubscriptions, &nevents);
  if (error != 0) {
    errno = uv__wasi_errno(error);
    return -1;
  }

  for (i = 0; i < nevents; i++) {
    const __wasi_event_t* event = &events[i];
    struct pollfd* pollfd;
    if (event->type != __WASI_EVENTTYPE_FD_READ &&
        event->type != __WASI_EVENTTYPE_FD_WRITE)
      continue;
    pollfd = (struct pollfd*)(uintptr_t)event->userdata;
    if (event->error == __WASI_ERRNO_BADF) {
      pollfd->revents |= POLLNVAL;
    } else if (event->error == __WASI_ERRNO_PIPE) {
      pollfd->revents |= POLLHUP;
    } else if (event->error != 0) {
      pollfd->revents |= POLLERR;
    } else {
      /* As poll(2): the readiness bits the caller asked for. */
      pollfd->revents |=
          pollfd->events & (event->type == __WASI_EVENTTYPE_FD_READ
                                ? (POLLIN | POLLRDNORM)
                                : (POLLOUT | POLLWRNORM));
      if (event->fd_readwrite.flags & __WASI_EVENTRWFLAGS_FD_READWRITE_HANGUP)
        pollfd->revents |= POLLHUP;
    }
  }

  count = 0;
  for (i = 0; i < nfds; i++) {
    if (fds[i].revents & POLLHUP) fds[i].revents &= ~(POLLOUT | POLLWRNORM);
    if (fds[i].revents != 0) count++;
  }
  return count;
}

static int uv__wasi_poll(struct pollfd* fds, nfds_t nfds, int timeout) {
  int ready;
  if (timeout == 0 || uv__wasi_can_suspend == NULL || !uv__wasi_can_suspend())
    return poll(fds, nfds, timeout);
  /* What is ready already needs no wait (and no trip through the host). */
  ready = poll(fds, nfds, 0);
  if (ready != 0) return ready;
  return uv__wasi_poll_suspending(fds, nfds, timeout);
}
#endif

/* Allocate or dynamically resize our poll fds array.  */
static void uv__pollfds_maybe_resize(uv_loop_t* loop) {
  size_t i;
  size_t n;
  struct pollfd* p;

  if (loop->poll_fds_used < loop->poll_fds_size)
    return;

  n = loop->poll_fds_size ? loop->poll_fds_size * 2 : 64;
  p = uv__reallocf(loop->poll_fds, n * sizeof(*loop->poll_fds));
  if (p == NULL)
    abort();

  loop->poll_fds = p;
  for (i = loop->poll_fds_size; i < n; i++) {
    loop->poll_fds[i].fd = -1;
    loop->poll_fds[i].events = 0;
    loop->poll_fds[i].revents = 0;
  }
  loop->poll_fds_size = n;
}

/* Primitive swap operation on poll fds array elements.  */
static void uv__pollfds_swap(uv_loop_t* loop, size_t l, size_t r) {
  struct pollfd pfd;
  pfd = loop->poll_fds[l];
  loop->poll_fds[l] = loop->poll_fds[r];
  loop->poll_fds[r] = pfd;
}

/* Add a watcher's fd to our poll fds array with its pending events.  */
static void uv__pollfds_add(uv_loop_t* loop, uv__io_t* w) {
  size_t i;
  struct pollfd* pe;

  /* If the fd is already in the set just update its events.  */
  assert(!loop->poll_fds_iterating);
  for (i = 0; i < loop->poll_fds_used; ++i) {
    if (loop->poll_fds[i].fd == w->fd) {
      loop->poll_fds[i].events = w->pevents;
      return;
    }
  }

  /* Otherwise, allocate a new slot in the set for the fd.  */
  uv__pollfds_maybe_resize(loop);
  pe = &loop->poll_fds[loop->poll_fds_used++];
  pe->fd = w->fd;
  pe->events = w->pevents;
}

/* Remove a watcher's fd from our poll fds array.  */
static void uv__pollfds_del(uv_loop_t* loop, int fd) {
  size_t i;
  assert(!loop->poll_fds_iterating);
  for (i = 0; i < loop->poll_fds_used;) {
    if (loop->poll_fds[i].fd == fd) {
      /* swap to last position and remove */
      --loop->poll_fds_used;
      uv__pollfds_swap(loop, i, loop->poll_fds_used);
      loop->poll_fds[loop->poll_fds_used].fd = -1;
      loop->poll_fds[loop->poll_fds_used].events = 0;
      loop->poll_fds[loop->poll_fds_used].revents = 0;
      /* This method is called with an fd of -1 to purge the invalidated fds,
       * so we may possibly have multiples to remove.
       */
      if (-1 != fd)
        return;
    } else {
      /* We must only increment the loop counter when the fds do not match.
       * Otherwise, when we are purging an invalidated fd, the value just
       * swapped here from the previous end of the array will be skipped.
       */
       ++i;
    }
  }
}


void uv__io_poll(uv_loop_t* loop, int timeout) {
  uv__loop_internal_fields_t* lfields;
  sigset_t* pset;
  sigset_t set;
  uint64_t time_base;
  uint64_t time_diff;
  struct uv__queue* q;
  uv__io_t* w;
  size_t i;
  unsigned int nevents;
  int nfds;
  int have_signals;
  struct pollfd* pe;
  int fd;
  int user_timeout;
  int reset_timeout;

  if (loop->nfds == 0) {
    assert(uv__queue_empty(&loop->watcher_queue));
    return;
  }

  lfields = uv__get_internal_fields(loop);

  /* Take queued watchers and add their fds to our poll fds array.  */
  while (!uv__queue_empty(&loop->watcher_queue)) {
    q = uv__queue_head(&loop->watcher_queue);
    uv__queue_remove(q);
    uv__queue_init(q);

    w = uv__queue_data(q, uv__io_t, watcher_queue);
    assert(w->pevents != 0);
    assert(w->fd >= 0);
    assert(w->fd < (int) loop->nwatchers);

    uv__pollfds_add(loop, w);

    w->events = w->pevents;
  }

  /* Prepare a set of signals to block around poll(), if any.  */
  pset = NULL;
  if (loop->flags & UV_LOOP_BLOCK_SIGPROF) {
    pset = &set;
    sigemptyset(pset);
    sigaddset(pset, SIGPROF);
  }

  assert(timeout >= -1);
  time_base = loop->time;

  if (lfields->flags & UV_METRICS_IDLE_TIME) {
    reset_timeout = 1;
    user_timeout = timeout;
    timeout = 0;
  } else {
    reset_timeout = 0;
  }

  /* Loop calls to poll() and processing of results.  If we get some
   * results from poll() but they turn out not to be interesting to
   * our caller then we need to loop around and poll() again.
   */
  for (;;) {
    uv__io_poll_prepare(loop, pset, timeout);
#if defined(__wasi__)
    nfds = uv__wasi_poll(loop->poll_fds, (nfds_t)loop->poll_fds_used, timeout);
#else
    nfds = poll(loop->poll_fds, (nfds_t)loop->poll_fds_used, timeout);
#endif
    uv__io_poll_check(loop, pset);

    if (nfds == 0) {
      if (reset_timeout != 0) {
        timeout = user_timeout;
        reset_timeout = 0;
        if (timeout == -1)
          continue;
        if (timeout > 0)
          goto update_timeout;
      }

      assert(timeout != -1);
      return;
    }

    if (nfds == -1) {
      if (errno != EINTR)
        abort();

      if (reset_timeout != 0) {
        timeout = user_timeout;
        reset_timeout = 0;
      }

      if (timeout == -1)
        continue;

      if (timeout == 0)
        return;

      /* Interrupted by a signal. Update timeout and poll again. */
      goto update_timeout;
    }

    /* Tell uv__platform_invalidate_fd not to manipulate our array
     * while we are iterating over it.
     */
    loop->poll_fds_iterating = 1;

    /* Initialize a count of events that we care about.  */
    nevents = 0;
    have_signals = 0;

    /* Loop over the entire poll fds array looking for returned events.  */
    for (i = 0; i < loop->poll_fds_used; i++) {
      pe = loop->poll_fds + i;
      fd = pe->fd;

      /* Skip invalidated events, see uv__platform_invalidate_fd.  */
      if (fd == -1)
        continue;

      assert(fd >= 0);
      assert((unsigned) fd < loop->nwatchers);

      w = loop->watchers[fd];

      if (w == NULL) {
        /* File descriptor that we've stopped watching, ignore.  */
        uv__platform_invalidate_fd(loop, fd);
        continue;
      }

      /* Filter out events that user has not requested us to watch
       * (e.g. POLLNVAL).
       */
      pe->revents &= w->pevents | POLLERR | POLLHUP;

      if (pe->revents != 0) {
        /* Run signal watchers last.  */
        if (w == &loop->signal_io_watcher) {
          have_signals = 1;
        } else {
          uv__metrics_update_idle_time(loop);
          uv__io_cb(loop, w, pe->revents);
        }

        nevents++;
      }
    }

    uv__metrics_inc_events(loop, nevents);
    if (reset_timeout != 0) {
      timeout = user_timeout;
      reset_timeout = 0;
      uv__metrics_inc_events_waiting(loop, nevents);
    }

    if (have_signals != 0) {
      uv__metrics_update_idle_time(loop);
      uv__signal_event(loop, &loop->signal_io_watcher, POLLIN);
    }

    loop->poll_fds_iterating = 0;

    /* Purge invalidated fds from our poll fds array.  */
    uv__pollfds_del(loop, -1);

    if (have_signals != 0)
      return;  /* Event loop should cycle now so don't poll again. */

    if (nevents != 0)
      return;

    if (timeout == 0)
      return;

    if (timeout == -1)
      continue;

update_timeout:
    assert(timeout > 0);

    time_diff = loop->time - time_base;
    if (time_diff >= (uint64_t) timeout)
      return;

    timeout -= time_diff;
  }
}

/* Remove the given fd from our poll fds array because no one
 * is interested in its events anymore.
 */
void uv__platform_invalidate_fd(uv_loop_t* loop, int fd) {
  size_t i;

  assert(fd >= 0);

  if (loop->poll_fds_iterating) {
    /* uv__io_poll is currently iterating.  Just invalidate fd.  */
    for (i = 0; i < loop->poll_fds_used; i++)
      if (loop->poll_fds[i].fd == fd) {
        loop->poll_fds[i].fd = -1;
        loop->poll_fds[i].events = 0;
        loop->poll_fds[i].revents = 0;
      }
  } else {
    /* uv__io_poll is not iterating.  Delete fd from the set.  */
    uv__pollfds_del(loop, fd);
  }
}

/* Check whether the given fd is supported by poll().  */
int uv__io_check_fd(uv_loop_t* loop, int fd) {
  struct pollfd p[1];
  int rv;

  p[0].fd = fd;
  p[0].events = POLLIN;

  do
    rv = poll(p, 1, 0);
  while (rv == -1 && (errno == EINTR || errno == EAGAIN));

  if (rv == -1)
    return UV__ERR(errno);

  if (p[0].revents & POLLNVAL)
    return UV_EINVAL;

  return 0;
}
