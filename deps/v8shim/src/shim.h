// Internals of the shim: the V8 API implemented on top of the JavaScript
// engine that hosts the WebAssembly instance.
//
// A JavaScript value lives in a table on the host side (host/v8shim.mjs in
// @progate/browser-v8). Its V8 address is the table index tagged like a heap
// object pointer, (index << 2) | 1; small integers are Smis as in V8. Local
// handles are slots in HandleScope blocks holding those addresses, exactly as
// in V8, so the inline parts of the headers keep working.

#ifndef V8SHIM_SRC_SHIM_H_
#define V8SHIM_SRC_SHIM_H_

#include <stdint.h>

#include "src/all-api.h"
#include "src/host.h"

namespace v8 {

// Converts between API objects and addresses. The headers make it a friend of
// Local and the other handle types, as V8's own Utils is.
class Utils {
 public:
  using Address = internal::Address;

  // The address an API object's `this` stands for: `this` is its slot.
  template <class T>
  static Address Open(const T* self) {
    return *reinterpret_cast<const Address*>(self);
  }
  template <class T>
  static Address Open(Local<T> handle) {
    return handle.IsEmpty() ? internal::kNullAddress : Open(*handle);
  }
  template <class T>
  static Address Open(MaybeLocal<T> handle) {
    Local<T> local;
    return handle.ToLocal(&local) ? Open(local) : internal::kNullAddress;
  }

  // A new handle in the current HandleScope. An empty address gives an empty
  // handle (a call that threw).
  template <class T>
  static Local<T> ToLocal(Address value) {
    if (value == internal::kNullAddress) return Local<T>();
    return Local<T>::FromSlot(HandleScope::CreateHandleForCurrentIsolate(value));
  }
  template <class T>
  static MaybeLocal<T> ToMaybeLocal(Address value) {
    return ToLocal<T>(value);
  }
};

namespace shim {

using Address = internal::Address;
using I = internal::Internals;

// Stops the program: the named part of the API is not implemented yet.
[[noreturn]] void Unimplemented(const char* what);

// Stops the program with a message, like V8's FATAL.
[[noreturn]] void Fatal(const char* format, ...);

#define V8SHIM_CHECK(condition)                                       \
  do {                                                                \
    if (!(condition))                                                 \
      ::v8::shim::Fatal("%s:%d: check failed: %s", __FILE__, __LINE__, \
                        #condition);                                  \
  } while (false)

// The one isolate. Its memory has V8's layout for the parts the headers read
// inline: roots, embedder data slots and the HandleScope data.
v8::Isolate* TheIsolate();

inline Address Root(int index) {
  return *I::GetRootSlot(TheIsolate(), index);
}
inline Address Undefined() { return Root(I::kUndefinedValueRootIndex); }
inline Address Null() { return Root(I::kNullValueRootIndex); }
inline Address True() { return Root(I::kTrueValueRootIndex); }
inline Address False() { return Root(I::kFalseValueRootIndex); }
inline Address Boolean(bool value) { return value ? True() : False(); }

inline bool IsSmi(Address value) { return !I::HasHeapObjectTag(value); }

}  // namespace shim
}  // namespace v8

#endif  // V8SHIM_SRC_SHIM_H_
