#ifndef UV_UNIX_WASI_ABI_H_
#define UV_UNIX_WASI_ABI_H_

/* The ABI of the `browser_wasi_async` host imports, as wasi_snapshot_preview1
 * spells it (wasi/api.h). BrowserOS keeps those names because the suspending
 * wait a JSPI thread can do is the one the host threads machinery offers in
 * that namespace; the libc here is musl, so this header carries the layout
 * itself instead of including <wasi/api.h>. Sizes and offsets are the ABI the
 * host reads and writes and are asserted below. */

#include <stddef.h>
#include <stdint.h>

typedef uint8_t __wasi_eventtype_t;
#define __WASI_EVENTTYPE_CLOCK (UINT8_C(0))
#define __WASI_EVENTTYPE_FD_READ (UINT8_C(1))
#define __WASI_EVENTTYPE_FD_WRITE (UINT8_C(2))

typedef uint16_t __wasi_errno_t;
#define __WASI_ERRNO_SUCCESS (UINT16_C(0))
#define __WASI_ERRNO_2BIG (UINT16_C(1))
#define __WASI_ERRNO_ACCES (UINT16_C(2))
#define __WASI_ERRNO_AGAIN (UINT16_C(6))
#define __WASI_ERRNO_BADF (UINT16_C(8))
#define __WASI_ERRNO_FAULT (UINT16_C(21))
#define __WASI_ERRNO_INTR (UINT16_C(27))
#define __WASI_ERRNO_INVAL (UINT16_C(28))
#define __WASI_ERRNO_IO (UINT16_C(29))
#define __WASI_ERRNO_NOMEM (UINT16_C(48))
#define __WASI_ERRNO_NOSYS (UINT16_C(52))
#define __WASI_ERRNO_NOTSUP (UINT16_C(58))
#define __WASI_ERRNO_PIPE (UINT16_C(64))

typedef uint32_t __wasi_fd_t;
typedef uint64_t __wasi_userdata_t;
typedef uint64_t __wasi_timestamp_t;
typedef uint64_t __wasi_filesize_t;
typedef uint32_t __wasi_clockid_t;
#define __WASI_CLOCKID_MONOTONIC (UINT32_C(1))

typedef uint16_t __wasi_eventrwflags_t;
#define __WASI_EVENTRWFLAGS_FD_READWRITE_HANGUP ((__wasi_eventrwflags_t)(1 << 0))

typedef struct __wasi_event_fd_readwrite_t {
  __wasi_filesize_t nbytes;
  __wasi_eventrwflags_t flags;
} __wasi_event_fd_readwrite_t;

_Static_assert(sizeof(__wasi_event_fd_readwrite_t) == 16, "witx calculated size");
_Static_assert(_Alignof(__wasi_event_fd_readwrite_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_event_fd_readwrite_t, nbytes) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_event_fd_readwrite_t, flags) == 8, "witx calculated offset");

typedef struct __wasi_event_t {
  __wasi_userdata_t userdata;
  __wasi_errno_t error;
  __wasi_eventtype_t type;
  __wasi_event_fd_readwrite_t fd_readwrite;
} __wasi_event_t;

_Static_assert(sizeof(__wasi_event_t) == 32, "witx calculated size");
_Static_assert(_Alignof(__wasi_event_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_event_t, userdata) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_event_t, error) == 8, "witx calculated offset");
_Static_assert(offsetof(__wasi_event_t, type) == 10, "witx calculated offset");
_Static_assert(offsetof(__wasi_event_t, fd_readwrite) == 16, "witx calculated offset");

typedef uint16_t __wasi_subclockflags_t;

typedef struct __wasi_subscription_clock_t {
  __wasi_clockid_t id;
  __wasi_timestamp_t timeout;
  __wasi_timestamp_t precision;
  __wasi_subclockflags_t flags;
} __wasi_subscription_clock_t;

_Static_assert(sizeof(__wasi_subscription_clock_t) == 32, "witx calculated size");
_Static_assert(_Alignof(__wasi_subscription_clock_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_subscription_clock_t, id) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_clock_t, timeout) == 8, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_clock_t, precision) == 16, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_clock_t, flags) == 24, "witx calculated offset");

typedef struct __wasi_subscription_fd_readwrite_t {
  __wasi_fd_t file_descriptor;
} __wasi_subscription_fd_readwrite_t;

_Static_assert(sizeof(__wasi_subscription_fd_readwrite_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_subscription_fd_readwrite_t) == 4, "witx calculated align");
_Static_assert(offsetof(__wasi_subscription_fd_readwrite_t, file_descriptor) == 0, "witx calculated offset");

typedef union __wasi_subscription_u_u_t {
  __wasi_subscription_clock_t clock;
  __wasi_subscription_fd_readwrite_t fd_read;
  __wasi_subscription_fd_readwrite_t fd_write;
} __wasi_subscription_u_u_t;
typedef struct __wasi_subscription_u_t {
  uint8_t tag;
  __wasi_subscription_u_u_t u;
} __wasi_subscription_u_t;

_Static_assert(sizeof(__wasi_subscription_u_t) == 40, "witx calculated size");
_Static_assert(_Alignof(__wasi_subscription_u_t) == 8, "witx calculated align");

typedef struct __wasi_subscription_t {
  __wasi_userdata_t userdata;
  __wasi_subscription_u_t u;
} __wasi_subscription_t;

_Static_assert(sizeof(__wasi_subscription_t) == 48, "witx calculated size");
_Static_assert(_Alignof(__wasi_subscription_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_subscription_t, userdata) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_t, u) == 8, "witx calculated offset");

#endif  /* UV_UNIX_WASI_ABI_H_ */
