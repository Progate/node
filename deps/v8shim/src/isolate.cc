// The isolate: one per program, standing for the host engine itself.

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "src/shim.h"
#include "src/state.h"

namespace v8 {

using shim::Address;
using I = internal::Internals;

namespace {

// The isolate's memory. Only the parts the headers read inline matter; they
// sit at V8's offsets.
constexpr size_t kIsolateSize = std::max<size_t>({
    I::kIsolateRootsOffset +
        (I::kEmptyStringRootIndex + 1) * internal::kApiSystemPointerSize,
    I::kIsolateEmbedderDataOffset +
        I::kNumIsolateDataSlots * internal::kApiSystemPointerSize,
    I::kIsolateHandleScopeDataOffset + internal::HandleScopeData::kSizeInBytes,
    I::kIsolateLongTaskStatsCounterOffset + sizeof(size_t),
});

Isolate* the_isolate = nullptr;

void FillRoots(Isolate* isolate) {
  for (int index : {I::kUndefinedValueRootIndex, I::kTheHoleValueRootIndex,
                    I::kNullValueRootIndex, I::kTrueValueRootIndex,
                    I::kFalseValueRootIndex, I::kEmptyStringRootIndex}) {
    *I::GetRootSlot(isolate, index) = shim::host::root(index);
  }
}

}  // namespace

namespace shim {

Isolate* TheIsolate() {
  if (the_isolate == nullptr) {
    // The memory has to exist before the embedder creates the isolate:
    // handles to roots can be made first (e.g. by v8::Undefined()).
    void* memory = calloc(1, kIsolateSize);
    V8SHIM_CHECK(memory != nullptr);
    the_isolate = static_cast<Isolate*>(memory);
    I::GetHandleScopeData(the_isolate)->Initialize();
    FillRoots(the_isolate);
  }
  return the_isolate;
}

}  // namespace shim

// --- Creating and entering ---------------------------------------------------

Isolate::CreateParams::CreateParams() = default;
Isolate::CreateParams::~CreateParams() = default;

Isolate* Isolate::Allocate() { return shim::TheIsolate(); }

Isolate* Isolate::Allocate(const IsolateGroup& group) { return Allocate(); }

void Isolate::Initialize(Isolate* isolate, const CreateParams& params) {
  shim::State& state = shim::GetState();
  state.cpp_heap = params.cpp_heap;
  state.array_buffer_allocator = params.array_buffer_allocator;
  if (params.array_buffer_allocator_shared)
    state.array_buffer_allocator = params.array_buffer_allocator_shared.get();
  if (state.platform != nullptr)
    state.foreground_task_runner =
        state.platform->GetForegroundTaskRunner(isolate);
}

Isolate* Isolate::New(const CreateParams& params) {
  Isolate* isolate = Allocate();
  Initialize(isolate, params);
  return isolate;
}

void Isolate::Dispose() { shim::GetState().foreground_task_runner.reset(); }
void Isolate::Deinitialize() {}
void Isolate::Free(Isolate* isolate) {}

Isolate* Isolate::GetCurrent() { return shim::TheIsolate(); }
Isolate* Isolate::TryGetCurrent() { return shim::TheIsolate(); }

void Isolate::Enter() {}
void Isolate::Exit() {}

// --- Contexts ----------------------------------------------------------------

Local<Context> Isolate::GetCurrentContext() {
  auto& entered = shim::GetState().entered_contexts;
  if (entered.empty()) return Local<Context>();
  return Utils::ToLocal<Context>(entered.back());
}

Local<Context> Isolate::GetEnteredOrMicrotaskContext() {
  return GetCurrentContext();
}

Local<Context> Isolate::GetIncumbentContext() { return GetCurrentContext(); }

bool Isolate::InContext() {
  return !shim::GetState().entered_contexts.empty();
}

// --- Exceptions and execution ----------------------------------------------

Local<Value> Isolate::ThrowException(Local<Value> exception) {
  shim::host::exception_throw(Utils::Open(exception));
  return Utils::ToLocal<Value>(shim::Undefined());
}

void Isolate::TerminateExecution() { shim::GetState().terminating = true; }

bool Isolate::IsExecutionTerminating() { return shim::GetState().terminating; }

void Isolate::CancelTerminateExecution() {
  shim::GetState().terminating = false;
}

Isolate::AllowJavascriptExecutionScope::AllowJavascriptExecutionScope(
    Isolate* isolate)
    : v8_isolate_(isolate) {}

Isolate::AllowJavascriptExecutionScope::~AllowJavascriptExecutionScope() =
    default;

Isolate::DisallowJavascriptExecutionScope::DisallowJavascriptExecutionScope(
    Isolate* isolate, OnFailure on_failure)
    : v8_isolate_(isolate), on_failure_(on_failure) {}

Isolate::DisallowJavascriptExecutionScope::~DisallowJavascriptExecutionScope() =
    default;

// Interrupts run as soon as the program returns to the event loop.
void Isolate::RequestInterrupt(InterruptCallback callback, void* data) {
  shim::GetState().interrupts.push_back({callback, data});
}

// --- Callbacks the embedder installs -----------------------------------------

void Isolate::SetFatalErrorHandler(FatalErrorCallback that) {
  shim::GetState().fatal_error_handler = that;
}

void Isolate::SetOOMErrorHandler(OOMErrorCallback that) {}

void Isolate::SetAbortOnUncaughtExceptionCallback(
    AbortOnUncaughtExceptionCallback callback) {
  shim::GetState().abort_on_uncaught_exception = callback;
}

void Isolate::SetPromiseRejectCallback(PromiseRejectCallback callback) {
  shim::GetState().promise_reject_callback = callback;
  shim::host::set_promise_reject_callback(callback != nullptr);
}

void Isolate::SetPromiseHook(PromiseHook hook) {}

void Isolate::SetPrepareStackTraceCallback(PrepareStackTraceCallback callback) {
  shim::GetState().prepare_stack_trace = callback;
  shim::host::set_prepare_stack_trace(callback != nullptr);
}

void Isolate::SetHostImportModuleDynamicallyCallback(
    HostImportModuleDynamicallyCallback callback) {
  shim::GetState().import_module_dynamically = callback;
}

void Isolate::SetHostImportModuleWithPhaseDynamicallyCallback(
    HostImportModuleWithPhaseDynamicallyCallback callback) {
  shim::GetState().import_module_with_phase_dynamically = callback;
}

void Isolate::SetHostInitializeImportMetaObjectCallback(
    HostInitializeImportMetaObjectCallback callback) {
  shim::GetState().initialize_import_meta = callback;
}

void Isolate::SetHostCreateShadowRealmContextCallback(
    HostCreateShadowRealmContextCallback callback) {}

void Isolate::SetModifyCodeGenerationFromStringsCallback(
    ModifyCodeGenerationFromStringsCallback2 callback) {}

void Isolate::SetAllowWasmCodeGenerationCallback(
    AllowWasmCodeGenerationCallback callback) {}

void Isolate::SetWasmStreamingCallback(WasmStreamingCallback callback) {}

bool Isolate::AddMessageListenerWithErrorLevel(MessageCallback that,
                                               int message_levels,
                                               Local<Value> data) {
  shim::GetState().message_listeners.push_back(
      {that, data.IsEmpty() ? shim::Undefined()
                            : shim::host::global_new(Utils::Open(data))});
  return true;
}

void Isolate::SetCaptureStackTraceForUncaughtExceptions(
    bool capture, int frame_limit, StackTrace::StackTraceOptions options) {}

void Isolate::SetMicrotasksPolicy(MicrotasksPolicy policy) {
  shim::GetState().microtasks_policy = policy;
}

void Isolate::SetIdle(bool is_idle) {}

void Isolate::SetStackLimit(uintptr_t stack_limit) {}

// --- Embedder data that is not inline ----------------------------------------

Local<Data> Isolate::GetContinuationPreservedEmbedderDataV2() {
  return Utils::ToLocal<Data>(shim::host::continuation_data());
}

void Isolate::SetContinuationPreservedEmbedderDataV2(Local<Data> data) {
  shim::host::set_continuation_data(
      data.IsEmpty() ? shim::Undefined() : Utils::Open(data));
}

// --- Heap: the host engine owns it -------------------------------------------

void Isolate::GetHeapStatistics(HeapStatistics* heap_statistics) {
  *heap_statistics = HeapStatistics();
}

size_t Isolate::NumberOfHeapSpaces() { return 0; }

bool Isolate::GetHeapSpaceStatistics(HeapSpaceStatistics* space_statistics,
                                     size_t index) {
  return false;
}

bool Isolate::GetHeapCodeAndMetadataStatistics(
    HeapCodeStatistics* code_statistics) {
  *code_statistics = HeapCodeStatistics();
  return true;
}

void Isolate::LowMemoryNotification() {}
void Isolate::ClearKeptObjects() {}
void Isolate::DumpAndResetStats() {}
void Isolate::AutomaticallyRestoreInitialHeapLimit(double threshold_percent) {}
void Isolate::DateTimeConfigurationChangeNotification(
    TimeZoneDetection time_zone_detection) {}

void Isolate::AddGCPrologueCallback(GCCallbackWithData callback, void* data,
                                    GCType gc_type) {}
void Isolate::RemoveGCPrologueCallback(GCCallbackWithData callback,
                                       void* data) {}
void Isolate::AddGCEpilogueCallback(GCCallbackWithData callback, void* data,
                                    GCType gc_type) {}
void Isolate::RemoveGCEpilogueCallback(GCCallbackWithData callback,
                                       void* data) {}

void Isolate::AddNearHeapLimitCallback(NearHeapLimitCallback callback,
                                       void* data) {}
void Isolate::RemoveNearHeapLimitCallback(NearHeapLimitCallback callback,
                                          size_t heap_limit) {}

uint64_t Isolate::GetHashSeed() { return 0; }

CppHeap* Isolate::GetCppHeap() const { return shim::GetState().cpp_heap; }

internal::Address* Isolate::GetDataFromSnapshotOnce(size_t index) {
  return nullptr;
}

// No sampling: the host engine's stack is not ours to walk.
void Isolate::GetStackSample(const RegisterState& state, void** frames,
                             size_t frames_limit, SampleInfo* sample_info) {
  sample_info->frames_count = 0;
}

bool Isolate::MeasureMemory(std::unique_ptr<MeasureMemoryDelegate> delegate,
                            MeasureMemoryExecution execution) {
  return false;
}

// There is one isolate and only its thread runs JavaScript.
void Locker::Initialize(Isolate* isolate) {
  has_lock_ = true;
  top_level_ = true;
  isolate_ = reinterpret_cast<internal::Isolate*>(isolate);
}

Locker::~Locker() = default;

std::string Isolate::GetDefaultLocale() { return "en-US"; }

// The host engine found a rejected promise with no handler (event 0), or a
// handler added to one it reported (event 1).
extern "C" __attribute__((export_name("v8shim_promise_reject"))) void
v8shim_promise_reject(int32_t event, Address promise, Address value) {
  PromiseRejectCallback callback = shim::GetState().promise_reject_callback;
  if (callback == nullptr) return;
  HandleScope scope(shim::TheIsolate());
  callback(PromiseRejectMessage(Utils::ToLocal<Promise>(promise),
                                static_cast<PromiseRejectEvent>(event),
                                Utils::ToLocal<Value>(value)));
}

// The host calls this when the host engine formats an error's stack, with the
// frames that are not the host's own. The result as a global entry the host
// disposes of, or 0 with the exception pending if the callback threw.
extern "C" __attribute__((export_name("v8shim_prepare_stack_trace"))) Address
v8shim_prepare_stack_trace(Address context, Address error, Address sites) {
  PrepareStackTraceCallback callback = shim::GetState().prepare_stack_trace;
  if (callback == nullptr) return 0;
  HandleScope scope(shim::TheIsolate());
  Address result = Utils::Open(callback(Utils::ToLocal<Context>(context),
                                        Utils::ToLocal<Value>(error),
                                        Utils::ToLocal<Array>(sites)));
  return result == 0 ? 0 : shim::host::global_new(result);
}

}  // namespace v8

namespace v8::shim {

State& GetState() {
  static auto* state = new State();
  return *state;
}

}  // namespace v8::shim
