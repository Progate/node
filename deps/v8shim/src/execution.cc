// Running code: contexts, scripts, exceptions, stack traces and microtasks.

#include <stdlib.h>
#include <string.h>

#include <new>
#include <unordered_map>
#include <vector>

#include "src/shim.h"
#include "src/state.h"

namespace v8 {

using shim::Address;
namespace host = shim::host;

namespace {

Address Self(const void* self) {
  return *reinterpret_cast<const Address*>(self);
}

Address OrZero(Local<Value> value) {
  return value.IsEmpty() ? 0 : Utils::Open(value);
}

// What a context keeps besides its global object: embedder data.
struct ContextData {
  std::vector<void*> pointers;
  std::vector<Address> values;  // Global entries, or 0.
  MicrotaskQueue* microtask_queue = nullptr;
  Address security_token = 0;
  bool allow_code_generation = true;
};

std::unordered_map<Address, ContextData>& Contexts() {
  static auto* contexts = new std::unordered_map<Address, ContextData>();
  return *contexts;
}

// A context's handle holds its global entry, which identifies it.
ContextData& DataOf(const Context* context) {
  return Contexts()[host::context_id(Self(context))];
}

constexpr int kEmbedderDataFields = 128;

}  // namespace

// --- Microtasks ----------------------------------------------------------------

namespace internal {

// The host engine runs microtasks when the program returns to its event loop.
// A checkpoint suspends the program there (JSPI) and resumes it once the
// queue is empty.
class MicrotaskQueue final : public v8::MicrotaskQueue {
 public:
  MicrotaskQueue() = default;

  void EnqueueMicrotask(v8::Isolate* isolate,
                        Local<Function> microtask) override {
    host::microtask_enqueue(Utils::Open(microtask));
  }

  void EnqueueMicrotask(v8::Isolate* isolate, MicrotaskCallback callback,
                        void* data) override {
    host::microtask_enqueue_native(reinterpret_cast<uintptr_t>(callback),
                                   reinterpret_cast<uintptr_t>(data), 0);
  }

  void AddMicrotasksCompletedCallback(
      MicrotasksCompletedCallbackWithData callback, void* data) override {
    completed_.push_back({callback, data});
  }

  void RemoveMicrotasksCompletedCallback(
      MicrotasksCompletedCallbackWithData callback, void* data) override {
    for (auto it = completed_.begin(); it != completed_.end(); ++it) {
      if (it->callback == callback && it->data == data) {
        completed_.erase(it);
        return;
      }
    }
  }

  void PerformCheckpoint(v8::Isolate* isolate) override {
    // Called from JS (node's runMicrotasks()), the host's queue runs once
    // that JS returns.
    if (running_ || !host::microtask_can_suspend()) return;
    running_ = true;
    host::microtask_checkpoint();
    running_ = false;
    for (const Completed& completed : completed_)
      completed.callback(isolate, completed.data);
  }

  bool IsRunningMicrotasks() const override { return running_; }

  int GetMicrotasksScopeDepth() const override { return 0; }

 private:
  struct Completed {
    MicrotasksCompletedCallbackWithData callback;
    void* data;
  };
  std::vector<Completed> completed_;
  bool running_ = false;
};

}  // namespace internal

namespace {

internal::MicrotaskQueue* NewQueue() { return new internal::MicrotaskQueue(); }

}  // namespace

std::unique_ptr<MicrotaskQueue> MicrotaskQueue::New(Isolate* isolate,
                                                    MicrotasksPolicy policy) {
  return std::unique_ptr<MicrotaskQueue>(NewQueue());
}

namespace {

MicrotaskQueue* DefaultMicrotaskQueue() {
  static auto* queue = NewQueue();
  return queue;
}

}  // namespace

// Native microtasks the host runs: callback(data).
extern "C" __attribute__((export_name("v8shim_run_microtask"))) void
v8shim_run_microtask(uintptr_t callback, uintptr_t data, Address value) {
  HandleScope scope(shim::TheIsolate());
  reinterpret_cast<MicrotaskCallback>(callback)(reinterpret_cast<void*>(data));
}

void Isolate::PerformMicrotaskCheckpoint() {
  DefaultMicrotaskQueue()->PerformCheckpoint(this);
}

void Isolate::EnqueueMicrotask(Local<Function> microtask) {
  DefaultMicrotaskQueue()->EnqueueMicrotask(this, microtask);
}

// --- Contexts ------------------------------------------------------------------

void Context::CheckCast(Data* that) {}

Local<Context> Context::New(
    Isolate* isolate, ExtensionConfiguration* extensions,
    MaybeLocal<ObjectTemplate> global_template,
    MaybeLocal<Value> global_object,
    DeserializeInternalFieldsCallback internal_fields_deserializer,
    MicrotaskQueue* microtask_queue,
    DeserializeContextDataCallback context_data_deserializer,
    DeserializeAPIWrapperCallback api_wrapper_deserializer) {
  Address context = host::context_new(Utils::Open(global_template),
                                      Utils::Open(global_object));
  if (context == 0) return Local<Context>();
  ContextData& data = Contexts()[context];
  data.microtask_queue =
      microtask_queue != nullptr ? microtask_queue : DefaultMicrotaskQueue();
  return Utils::ToLocal<Context>(context);
}

// There is no snapshot: every context is made with Context::New.
MaybeLocal<Context> Context::FromSnapshot(
    Isolate* isolate, size_t context_snapshot_index,
    DeserializeInternalFieldsCallback internal_fields_deserializer,
    ExtensionConfiguration* extensions, MaybeLocal<Value> global_object,
    MicrotaskQueue* microtask_queue,
    DeserializeContextDataCallback context_data_deserializer,
    DeserializeAPIWrapperCallback api_wrapper_deserializer) {
  return MaybeLocal<Context>();
}

internal::Address* Context::GetDataFromSnapshotOnce(size_t index) {
  return nullptr;
}

Local<Object> Context::Global() {
  return Utils::ToLocal<Object>(host::context_global(Self(this)));
}

void Context::Enter() {
  shim::GetState().entered_contexts.push_back(host::context_id(Self(this)));
}

void Context::Exit() {
  auto& entered = shim::GetState().entered_contexts;
  V8SHIM_CHECK(!entered.empty());
  entered.pop_back();
}

uint32_t Context::GetNumberOfEmbedderDataFields() {
  return kEmbedderDataFields;
}

void Context::SetEmbedderData(int index, Local<Value> value) {
  ContextData& data = DataOf(this);
  if (data.values.size() <= static_cast<size_t>(index))
    data.values.resize(index + 1);
  if (data.values[index]) host::global_dispose(data.values[index]);
  data.values[index] = value.IsEmpty() ? 0 : host::global_new(Utils::Open(value));
}

Local<Value> Context::SlowGetEmbedderData(int index) {
  ContextData& data = DataOf(this);
  if (static_cast<size_t>(index) >= data.values.size() || !data.values[index])
    return Utils::ToLocal<Value>(shim::Undefined());
  return Utils::ToLocal<Value>(data.values[index]);
}

void Context::SetAlignedPointerInEmbedderData(int index, void* value,
                                              EmbedderDataTypeTag tag) {
  ContextData& data = DataOf(this);
  if (data.pointers.size() <= static_cast<size_t>(index))
    data.pointers.resize(index + 1);
  data.pointers[index] = value;
}

void* Context::SlowGetAlignedPointerFromEmbedderData(int index,
                                                     EmbedderDataTypeTag tag) {
  ContextData& data = DataOf(this);
  if (static_cast<size_t>(index) >= data.pointers.size()) return nullptr;
  return data.pointers[index];
}

MicrotaskQueue* Context::GetMicrotaskQueue() {
  MicrotaskQueue* queue = DataOf(this).microtask_queue;
  return queue != nullptr ? queue : DefaultMicrotaskQueue();
}

void Context::SetSecurityToken(Local<Value> token) {
  ContextData& data = DataOf(this);
  if (data.security_token) host::global_dispose(data.security_token);
  data.security_token = host::global_new(Utils::Open(token));
}

Local<Value> Context::GetSecurityToken() {
  Address token = DataOf(this).security_token;
  return Utils::ToLocal<Value>(token ? token : shim::Undefined());
}

void Context::AllowCodeGenerationFromStrings(bool allow) {
  DataOf(this).allow_code_generation = allow;
}

bool Context::IsCodeGenerationFromStringsAllowed() const {
  return Contexts()[host::context_id(Self(this))].allow_code_generation;
}

Local<Object> Context::GetExtrasBindingObject() {
  return Utils::ToLocal<Object>(host::context_extras(Self(this)));
}

// Promise hooks need the engine's promise internals; the host has none.
void Context::SetPromiseHooks(Local<Function> init_hook,
                              Local<Function> before_hook,
                              Local<Function> after_hook,
                              Local<Function> resolve_hook) {}

// --- Scripts ---------------------------------------------------------------------

void ScriptOrigin::VerifyHostDefinedOptions() const {}

namespace {

Address CompileScript(Local<String> source, Local<Value> name, int line,
                      int column, Local<Data> host_defined_options) {
  return host::script_compile(Utils::Open(source), OrZero(name), line, column,
                              Utils::Open(host_defined_options));
}

}  // namespace

MaybeLocal<Script> Script::Compile(Local<Context> context, Local<String> source,
                                   ScriptOrigin* origin) {
  if (origin == nullptr)
    return Utils::ToMaybeLocal<Script>(
        CompileScript(source, Local<Value>(), 0, 0, Local<Data>()));
  return Utils::ToMaybeLocal<Script>(CompileScript(
      source, origin->ResourceName(), origin->LineOffset(),
      origin->ColumnOffset(), origin->GetHostDefinedOptions()));
}

MaybeLocal<Value> Script::Run(Local<Context> context) {
  return Utils::ToMaybeLocal<Value>(host::script_run(Self(this)));
}

MaybeLocal<UnboundScript> ScriptCompiler::CompileUnboundScript(
    Isolate* isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  if (source->cached_data) source->cached_data->rejected = true;
  return Utils::ToMaybeLocal<UnboundScript>(
      CompileScript(source->source_string, source->resource_name,
                    source->resource_line_offset, source->resource_column_offset,
                    source->host_defined_options));
}

Local<Script> UnboundScript::BindToCurrentContext() {
  return Utils::ToLocal<Script>(Self(this));
}

Local<Value> UnboundScript::GetSourceURL() {
  return Utils::ToLocal<Value>(host::script_url(Self(this), 0));
}

Local<Value> UnboundScript::GetSourceMappingURL() {
  return Utils::ToLocal<Value>(host::script_url(Self(this), 1));
}

MaybeLocal<Function> ScriptCompiler::CompileFunction(
    Local<Context> context, Source* source, size_t arguments_count,
    Local<String> arguments[], size_t context_extension_count,
    Local<Object> context_extensions[], CompileOptions options,
    NoCacheReason no_cache_reason) {
  if (source->cached_data) source->cached_data->rejected = true;
  std::vector<Address> parameters(arguments_count);
  for (size_t i = 0; i < arguments_count; i++)
    parameters[i] = Utils::Open(arguments[i]);
  std::vector<Address> extensions(context_extension_count);
  for (size_t i = 0; i < context_extension_count; i++)
    extensions[i] = Utils::Open(context_extensions[i]);
  return Utils::ToMaybeLocal<Function>(host::compile_function(
      Utils::Open(source->source_string), OrZero(source->resource_name),
      source->resource_line_offset, source->resource_column_offset,
      parameters.data(), static_cast<uint32_t>(arguments_count),
      extensions.data(), static_cast<uint32_t>(context_extension_count),
      Utils::Open(source->host_defined_options)));
}

// There is no bytecode to cache. A cache is empty, and is rejected when it
// is given back.
uint32_t ScriptCompiler::CachedDataVersionTag() { return 0; }

namespace {

ScriptCompiler::CachedData* EmptyCodeCache() {
  return new ScriptCompiler::CachedData(nullptr, 0);
}

}  // namespace

ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCache(
    Local<UnboundScript> unbound_script) {
  return EmptyCodeCache();
}

ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCache(
    Local<UnboundModuleScript> unbound_module_script) {
  return EmptyCodeCache();
}

ScriptCompiler::CachedData* ScriptCompiler::CreateCodeCacheForFunction(
    Local<Function> function) {
  return EmptyCodeCache();
}

ScriptCompiler::CachedData::CachedData(const uint8_t* data, int length,
                                       BufferPolicy buffer_policy)
    : data(data),
      length(length),
      rejected(false),
      buffer_policy(buffer_policy) {}

ScriptCompiler::CachedData::~CachedData() {
  if (buffer_policy == BufferOwned) delete[] data;
}

// --- Exceptions ----------------------------------------------------------------

namespace {

constexpr uint8_t kVerbose = 1 << 0;
constexpr uint8_t kRethrow = 1 << 1;

}  // namespace

// The host holds a thrown exception as pending until a TryCatch looks at it.
// The innermost TryCatch then takes it (exception_ holds a global entry), as
// V8 stores a caught exception in the TryCatch: a TryCatch opened later, while
// handling it, does not see it. An exception that is pending when a TryCatch
// opens belongs to the enclosing TryCatch, or, with none, waits until this one
// closes (js_stack_comparable_address_ holds it meanwhile).

namespace {

void TakePending(void*& slot) {
  Address pending = host::exception_pending();
  if (pending == 0) return;
  if (slot != nullptr)
    host::global_dispose(reinterpret_cast<Address>(slot));
  slot = reinterpret_cast<void*>(host::global_new(pending));
  host::exception_clear();
}

Address Caught(void* slot) { return reinterpret_cast<Address>(slot); }

// A listener added without data gets the exception, as in V8.
void ReportToListeners(Address exception) {
  HandleScope scope(shim::TheIsolate());
  Local<v8::Message> message =
      Utils::ToLocal<v8::Message>(host::message_new(exception));
  for (const shim::MessageListener& listener :
       shim::GetState().message_listeners) {
    Local<Value> data = Utils::ToLocal<Value>(listener.data);
    if (data->IsUndefined()) data = Utils::ToLocal<Value>(exception);
    listener.callback(message, data);
  }
}

}  // namespace

TryCatch::TryCatch(Isolate* isolate)
    : i_isolate_(reinterpret_cast<internal::Isolate*>(isolate)),
      next_(shim::GetState().try_catch_top),
      exception_(nullptr),
      message_obj_(nullptr),
      js_stack_comparable_address_(0),
      flags_(0) {
  if (next_ != nullptr) {
    TakePending(next_->exception_);
  } else if (Address pending = host::exception_pending()) {
    js_stack_comparable_address_ = host::global_new(pending);
    host::exception_clear();
  }
  shim::GetState().try_catch_top = this;
}

TryCatch::~TryCatch() {
  shim::GetState().try_catch_top = next_;
  TakePending(exception_);
  if (Address caught = Caught(exception_)) {
    if (flags_ & kRethrow)
      host::exception_throw(caught);
    else if (flags_ & kVerbose)
      ReportToListeners(caught);
    host::global_dispose(caught);
  }
  if (Address waiting = js_stack_comparable_address_) {
    if (host::exception_pending() == 0) host::exception_throw(waiting);
    host::global_dispose(waiting);
  }
}

// The host calls this when an exception comes back to C++ with no JS frames
// on the stack to catch it. Without a TryCatch, V8 reports it to the message
// listeners (node's uncaught exception handler) and drops it.
extern "C" __attribute__((export_name("v8shim_uncaught_exception"))) void
v8shim_uncaught_exception() {
  if (shim::GetState().try_catch_top != nullptr) return;
  Address pending = host::exception_pending();
  if (pending == 0) return;
  Address exception = host::global_new(pending);
  host::exception_clear();
  ReportToListeners(exception);
  host::global_dispose(exception);
}

// An exception is on its way out of an API call: thrown and not yet taken by a
// TryCatch.
bool Isolate::HasPendingException() { return host::exception_pending() != 0; }

bool TryCatch::HasCaught() const {
  TakePending(const_cast<TryCatch*>(this)->exception_);
  return exception_ != nullptr;
}

bool TryCatch::CanContinue() const { return true; }

bool TryCatch::HasTerminated() const { return false; }

Local<Value> TryCatch::ReThrow() {
  TakePending(exception_);
  flags_ |= kRethrow;
  return Utils::ToLocal<Value>(shim::Undefined());
}

Local<Value> TryCatch::Exception() const {
  TakePending(const_cast<TryCatch*>(this)->exception_);
  Address caught = Caught(exception_);
  // A local in the current scope, as in V8: node keeps using it after the
  // TryCatch is gone, and the TryCatch disposes of its own entry.
  return caught ? Utils::ToLocal<Value>(host::scope_push(caught))
                : Local<Value>();
}

Local<v8::Message> TryCatch::Message() const {
  TakePending(const_cast<TryCatch*>(this)->exception_);
  Address caught = Caught(exception_);
  if (caught == 0) return Local<v8::Message>();
  return Utils::ToLocal<v8::Message>(host::message_new(caught));
}

void TryCatch::Reset() {
  TakePending(exception_);
  if (Address caught = Caught(exception_)) host::global_dispose(caught);
  exception_ = nullptr;
  flags_ &= ~kRethrow;
}

void TryCatch::SetVerbose(bool value) {
  flags_ = value ? (flags_ | kVerbose) : (flags_ & ~kVerbose);
}

bool TryCatch::IsVerbose() const { return (flags_ & kVerbose) != 0; }

namespace {

Local<Value> NewError(host::ErrorKind kind, Local<String> message,
                      Local<Value> options) {
  return Utils::ToLocal<Value>(
      host::error_new(kind, Utils::Open(message), OrZero(options)));
}

}  // namespace

Local<Value> Exception::Error(Local<String> message, Local<Value> options) {
  return NewError(host::kError, message, options);
}
Local<Value> Exception::RangeError(Local<String> message, Local<Value> options) {
  return NewError(host::kRangeError, message, options);
}
Local<Value> Exception::SyntaxError(Local<String> message,
                                    Local<Value> options) {
  return NewError(host::kSyntaxError, message, options);
}
Local<Value> Exception::TypeError(Local<String> message, Local<Value> options) {
  return NewError(host::kTypeError, message, options);
}

Local<Message> Exception::CreateMessage(Isolate* isolate,
                                        Local<Value> exception) {
  return Utils::ToLocal<Message>(host::message_new(Utils::Open(exception)));
}

// --- Messages and stack traces ---------------------------------------------------

Local<String> Message::Get() const {
  return Utils::ToLocal<String>(host::message_part(Self(this), host::kMessageText));
}

ScriptOrigin Message::GetScriptOrigin() const {
  return ScriptOrigin(
      Utils::ToLocal<Value>(
          host::message_part(Self(this), host::kMessageResourceName)),
      0, 0, false, -1, Undefined(Isolate::GetCurrent()));
}

Local<Value> Message::GetScriptResourceName() const {
  return Utils::ToLocal<Value>(
      host::message_part(Self(this), host::kMessageResourceName));
}

Maybe<int> Message::GetLineNumber(Local<Context> context) const {
  return Just(host::message_number(Self(this), host::kMessageLine));
}

int Message::GetStartColumn() const {
  return host::message_number(Self(this), host::kMessageStartColumn);
}

int Message::GetEndColumn() const {
  return host::message_number(Self(this), host::kMessageEndColumn);
}

MaybeLocal<String> Message::GetSourceLine(Local<Context> context) const {
  return Utils::ToMaybeLocal<String>(
      host::message_part(Self(this), host::kMessageSourceLine));
}

Local<StackTrace> Message::GetStackTrace() const {
  Address trace = host::message_part(Self(this), host::kMessageStackTrace);
  return trace == shim::Undefined() ? Local<StackTrace>()
                                    : Utils::ToLocal<StackTrace>(trace);
}

int Message::ErrorLevel() const { return Isolate::kMessageError; }

Local<StackTrace> StackTrace::CurrentStackTrace(Isolate* isolate,
                                                int frame_limit,
                                                StackTraceOptions options) {
  return Utils::ToLocal<StackTrace>(host::stack_trace_current(frame_limit));
}

int StackTrace::GetFrameCount() const {
  return static_cast<int>(host::array_length(Self(this)));
}

Local<StackFrame> StackTrace::GetFrame(Isolate* isolate, uint32_t index) const {
  return Utils::ToLocal<StackFrame>(
      host::object_get(Self(this), host::number_new(index)));
}

Location StackFrame::GetLocation() const {
  return Location(host::frame_number(Self(this), host::kFrameLine),
                  host::frame_number(Self(this), host::kFrameColumn));
}

int StackFrame::GetScriptId() const {
  return host::frame_number(Self(this), host::kFrameScriptId);
}

Local<String> StackFrame::GetScriptName() const {
  return Utils::ToLocal<String>(host::frame_part(Self(this), host::kFrameScriptName));
}

Local<String> StackFrame::GetScriptNameOrSourceURL() const {
  return GetScriptName();
}

Local<String> StackFrame::GetFunctionName() const {
  return Utils::ToLocal<String>(
      host::frame_part(Self(this), host::kFrameFunctionName));
}

bool StackFrame::IsEval() const {
  return host::frame_number(Self(this), host::kFrameIsEval) != 0;
}

}  // namespace v8
