// What the host JavaScript engine provides to the shim: the `v8shim` imports.
// The host side is host/v8shim.mjs in @progate/browser-v8; this file is the
// contract between the two.
//
// Values are V8 addresses (see shim.h): a host table index tagged as a heap
// object, or a Smi. A function that can throw returns 0 (kNullAddress) or -1
// and leaves the exception pending on the host (exception_pending).

#ifndef V8SHIM_SRC_HOST_H_
#define V8SHIM_SRC_HOST_H_

#include <stddef.h>
#include <stdint.h>

#define V8SHIM_IMPORT(name) \
  __attribute__((import_module("v8shim"), import_name(#name)))

namespace v8::shim::host {

using Address = uintptr_t;

extern "C" {

// --- Table -----------------------------------------------------------------
// Scoped entries form a stack: scope_exit(mark) drops everything above mark.
V8SHIM_IMPORT(scope_enter) uint32_t scope_enter();
V8SHIM_IMPORT(scope_exit) void scope_exit(uint32_t mark);
// A new scoped entry holding `value` (keeps it alive in the current scope).
V8SHIM_IMPORT(scope_push) Address scope_push(Address value);
// Makes the entry `entry` (from scope_push) hold `value` instead.
V8SHIM_IMPORT(table_set) void table_set(Address entry, Address value);
// Global entries stay until disposed.
V8SHIM_IMPORT(global_new) Address global_new(Address value);
V8SHIM_IMPORT(global_dispose) void global_dispose(Address global);
// Weak globals call v8shim_weak_callback(node) once the value is collected.
V8SHIM_IMPORT(global_make_weak) void global_make_weak(Address global, Address* node);
V8SHIM_IMPORT(global_clear_weak) void global_clear_weak(Address global);
// Roots: undefined, the hole, null, true, false and "" (Internals root order).
V8SHIM_IMPORT(root) Address root(int32_t index);

// --- Identity and type -----------------------------------------------------
V8SHIM_IMPORT(same_value) int32_t same_value(Address a, Address b);
// V8's instance type for the inline checks of v8-internal.h: strings are
// below kFirstNonstringType, oddballs are kOddballType, objects kJSObjectType.
V8SHIM_IMPORT(instance_type) int32_t instance_type(Address value);
V8SHIM_IMPORT(oddball_kind) int32_t oddball_kind(Address value);
// A stable address per kind of value, standing in for a map.
V8SHIM_IMPORT(load_map) Address load_map(Address value);

// --- Exceptions ---------------------------------------------------------------
// One exception can be pending: a call that threw, or Isolate::ThrowException.
// A callback returning to JavaScript throws it.
V8SHIM_IMPORT(exception_throw) void exception_throw(Address exception);
V8SHIM_IMPORT(exception_pending) Address exception_pending();
V8SHIM_IMPORT(exception_clear) void exception_clear();

// --- Isolate callbacks ---------------------------------------------------------
// Whether the host should call v8shim_promise_reject / v8shim_prepare_stack_trace.
V8SHIM_IMPORT(set_promise_reject_callback) void set_promise_reject_callback(int32_t enabled);
V8SHIM_IMPORT(set_prepare_stack_trace) void set_prepare_stack_trace(int32_t enabled);

// --- Calls into C++ ----------------------------------------------------------
// A callback stored its return value into the frame slot at `slot`.
V8SHIM_IMPORT(set_return_value) void set_return_value(Address* slot, Address value);

}  // extern "C"

}  // namespace v8::shim::host

#endif  // V8SHIM_SRC_HOST_H_
