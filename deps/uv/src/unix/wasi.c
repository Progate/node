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

/* WebAssembly (wasm32-wasip1-threads) running on BrowserOS.
 *
 * There is one linear memory of at most 4 GiB and no /proc. The event loop is
 * the poll(2) one (posix-poll.c) and processes are started with
 * posix_spawn(3) (process.c).
 */

#include "uv.h"
#include "internal.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#define UV__WASM_PAGE_SIZE 65536
#define UV__WASM_MEMORY_MAX (4ULL * 1024 * 1024 * 1024)

static char* process_argv0;
static char* process_title;
static size_t process_title_length;


char** uv_setup_args(int argc, char** argv) {
  if (argc > 0 && argv[0] != NULL)
    process_argv0 = uv__strdup(argv[0]);
  return argv;
}


void uv__process_title_cleanup(void) {
  uv__free(process_argv0);
  uv__free(process_title);
  process_argv0 = NULL;
  process_title = NULL;
}


int uv_set_process_title(const char* title) {
  char* copy;

  copy = uv__strdup(title);
  if (copy == NULL)
    return UV_ENOMEM;
  uv__free(process_title);
  process_title = copy;
  process_title_length = strlen(copy);
  return 0;
}


int uv_get_process_title(char* buffer, size_t size) {
  const char* title;
  size_t length;

  if (buffer == NULL || size == 0)
    return UV_EINVAL;

  title = process_title != NULL ? process_title : process_argv0;
  if (title == NULL)
    title = "";
  length = strlen(title);
  if (size <= length)
    return UV_ENOBUFS;
  memcpy(buffer, title, length + 1);
  return 0;
}


void uv_loadavg(double avg[3]) {
  avg[0] = 0;
  avg[1] = 0;
  avg[2] = 0;
}


/* The program's path is its argv[0], looked up in PATH like a shell does. */
int uv_exepath(char* buffer, size_t* size) {
  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;
  if (process_argv0 == NULL)
    return UV_EINVAL;
  return uv__search_path(process_argv0, buffer, size);
}


static uint64_t uv__wasm_memory_used(void) {
  return (uint64_t) __builtin_wasm_memory_size(0) * UV__WASM_PAGE_SIZE;
}


uint64_t uv_get_total_memory(void) {
  return UV__WASM_MEMORY_MAX;
}


uint64_t uv_get_free_memory(void) {
  return UV__WASM_MEMORY_MAX - uv__wasm_memory_used();
}


uint64_t uv_get_constrained_memory(void) {
  return UV__WASM_MEMORY_MAX;
}


uint64_t uv_get_available_memory(void) {
  return uv_get_free_memory();
}


int uv_resident_set_memory(size_t* rss) {
  *rss = (size_t) uv__wasm_memory_used();
  return 0;
}


int uv_uptime(double* uptime) {
  struct timespec now;

  if (clock_gettime(CLOCK_MONOTONIC, &now))
    return UV__ERR(errno);
  *uptime = now.tv_sec + now.tv_nsec / 1e9;
  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  uv_cpu_info_t* cpu_info;
  int n;
  int i;

  n = (int) uv_available_parallelism();
  cpu_info = uv__calloc(n, sizeof(*cpu_info));
  if (cpu_info == NULL)
    return UV_ENOMEM;

  for (i = 0; i < n; i++) {
    cpu_info[i].model = uv__strdup("wasm32");
    if (cpu_info[i].model == NULL) {
      uv_free_cpu_info(cpu_info, i);
      return UV_ENOMEM;
    }
  }

  *cpu_infos = cpu_info;
  *count = n;
  return 0;
}


/* BrowserOS has one network interface, the loopback. */
int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  uv_interface_address_t* address;
  struct sockaddr_in* in4;
  struct sockaddr_in6* in6;

  *addresses = uv__calloc(2, sizeof(**addresses));
  if (*addresses == NULL)
    return UV_ENOMEM;
  *count = 2;

  address = &(*addresses)[0];
  address->name = uv__strdup("lo");
  address->is_internal = 1;
  in4 = &address->address.address4;
  in4->sin_family = AF_INET;
  in4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  in4 = &address->netmask.netmask4;
  in4->sin_family = AF_INET;
  in4->sin_addr.s_addr = htonl(0xff000000);

  address = &(*addresses)[1];
  address->name = uv__strdup("lo");
  address->is_internal = 1;
  in6 = &address->address.address6;
  in6->sin6_family = AF_INET6;
  in6->sin6_addr = in6addr_loopback;
  in6 = &address->netmask.netmask6;
  in6->sin6_family = AF_INET6;
  memset(&in6->sin6_addr, 0xff, sizeof(in6->sin6_addr));

  if ((*addresses)[0].name == NULL || (*addresses)[1].name == NULL) {
    uv_free_interface_addresses(*addresses, *count);
    return UV_ENOMEM;
  }
  return 0;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
                                 int count) {
  int i;

  for (i = 0; i < count; i++)
    uv__free(addresses[i].name);
  uv__free(addresses);
}
