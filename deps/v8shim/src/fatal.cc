#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "src/shim.h"

namespace v8::shim {

void Unimplemented(const char* what) {
  fprintf(stderr, "browser-v8: not implemented: %s\n", what);
  fflush(stderr);
  abort();
}

void Fatal(const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  fprintf(stderr, "browser-v8: ");
  vfprintf(stderr, format, arguments);
  fprintf(stderr, "\n");
  va_end(arguments);
  fflush(stderr);
  abort();
}

}  // namespace v8::shim
