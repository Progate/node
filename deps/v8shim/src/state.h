// What the isolate remembers: entered contexts and the callbacks the embedder
// installs.

#ifndef V8SHIM_SRC_STATE_H_
#define V8SHIM_SRC_STATE_H_

#include <vector>

#include "src/shim.h"

namespace v8::shim {

struct MessageListener {
  MessageCallback callback;
  Address data;  // A global entry.
};

struct Interrupt {
  InterruptCallback callback;
  void* data;
};

struct State {
  ArrayBuffer::Allocator* array_buffer_allocator = nullptr;

  // Global entries of the contexts entered with Context::Enter(), innermost
  // last.
  std::vector<Address> entered_contexts;

  bool terminating = false;
  MicrotasksPolicy microtasks_policy = MicrotasksPolicy::kAuto;
  std::vector<Interrupt> interrupts;
  Address continuation_data = 0;  // A global entry, or 0.

  FatalErrorCallback fatal_error_handler = nullptr;
  Isolate::AbortOnUncaughtExceptionCallback abort_on_uncaught_exception =
      nullptr;
  PromiseRejectCallback promise_reject_callback = nullptr;
  PrepareStackTraceCallback prepare_stack_trace = nullptr;
  HostImportModuleDynamicallyCallback import_module_dynamically = nullptr;
  HostImportModuleWithPhaseDynamicallyCallback
      import_module_with_phase_dynamically = nullptr;
  HostInitializeImportMetaObjectCallback initialize_import_meta = nullptr;
  std::vector<MessageListener> message_listeners;
};

State& GetState();

}  // namespace v8::shim

#endif  // V8SHIM_SRC_STATE_H_
