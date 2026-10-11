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

#ifndef UV_POSIX_H
#define UV_POSIX_H

#if defined(__wasi__)
/* BrowserOS gives WASI programs inotify(7); uv_fs_event_t uses it as on
 * Linux (src/unix/wasi-inotify.c). */
#define UV_PLATFORM_LOOP_FIELDS                                               \
  struct pollfd* poll_fds;                                                    \
  size_t poll_fds_used;                                                       \
  size_t poll_fds_size;                                                       \
  unsigned char poll_fds_iterating;                                           \
  uv__io_t inotify_read_watcher;                                              \
  void* inotify_watchers;                                                     \
  int inotify_fd;                                                             \

#define UV_PLATFORM_FS_EVENT_FIELDS                                           \
  struct uv__queue watchers;                                                  \
  int wd;                                                                     \

#else
#define UV_PLATFORM_LOOP_FIELDS                                               \
  struct pollfd* poll_fds;                                                    \
  size_t poll_fds_used;                                                       \
  size_t poll_fds_size;                                                       \
  unsigned char poll_fds_iterating;                                           \

#endif

#endif /* UV_POSIX_H */
