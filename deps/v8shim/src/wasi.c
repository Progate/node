// What the C libraries node links (OpenSSL) ask of the OS and WASI has not.

#include <errno.h>
#include <stddef.h>

// OpenSSL's secure heap locks its pages; there is no paging to lock them
// against. Failing makes it carry on without, as on a system that refuses.
int mlock(const void* addr, size_t len) {
  (void)addr;
  (void)len;
  errno = ENOSYS;
  return -1;
}
