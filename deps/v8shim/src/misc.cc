// Small parts of the API whose classes hold V8 internals by unique_ptr or
// have const members, so they cannot be generated stubs.

#include <memory>

#include "src/shim.h"

namespace v8 {

// The internals these classes point to. Nothing in the shim fills them.
struct CalleeSavedRegisters {};
namespace internal {
class BackgroundDeserializeTask {};
class SharedObjectConveyorHandles {};
}  // namespace internal
namespace base {
class Mutex {};
}  // namespace base

RegisterState::RegisterState()
    : pc(nullptr), sp(nullptr), fp(nullptr), lr(nullptr) {}

RegisterState::~RegisterState() = default;

RegisterState::RegisterState(const RegisterState& other) { *this = other; }

RegisterState& RegisterState::operator=(const RegisterState& other) {
  if (&other != this) {
    pc = other.pc;
    sp = other.sp;
    fp = other.fp;
    lr = other.lr;
    if (other.callee_saved)
      callee_saved = std::make_unique<CalleeSavedRegisters>(*other.callee_saved);
    else
      callee_saved.reset();
  }
  return *this;
}

ScriptCompiler::ConsumeCodeCacheTask::~ConsumeCodeCacheTask() = default;

CFunctionInfo::CFunctionInfo(const CTypeInfo& return_info,
                             unsigned int arg_count, const CTypeInfo* arg_info,
                             Int64Representation repr)
    : return_info_(return_info),
      repr_(repr),
      arg_count_(arg_count),
      arg_info_(arg_info) {}

SharedValueConveyor::SharedValueConveyor(SharedValueConveyor&& other) noexcept =
    default;

SharedValueConveyor::~SharedValueConveyor() = default;

SharedValueConveyor& SharedValueConveyor::operator=(
    SharedValueConveyor&& other) noexcept = default;

// The host engine checks types itself; these casts have nothing to verify.
void DictionaryTemplate::CheckCast(Data* that) {}
void Module::CheckCast(Data* that) {}
void ModuleRequest::CheckCast(Data* that) {}
void Set::CheckCast(Value* that) {}
void Signature::CheckCast(Data* that) {}
void WasmMemoryObject::CheckCast(Value* that) {}
void WasmModuleObject::CheckCast(Value* that) {}

// A compiled module goes to another isolate (node's worker_threads) the way a
// SharedArrayBuffer does: the hosts pass the WebAssembly.Module itself, named
// by a serial number, and the CompiledWasmModule carries that number.
namespace {

uint32_t SerialOf(const std::shared_ptr<internal::wasm::NativeModule>& module) {
  return *reinterpret_cast<const uint32_t*>(module.get());
}

}  // namespace

CompiledWasmModule::CompiledWasmModule(
    std::shared_ptr<internal::wasm::NativeModule> native_module,
    std::string source_url)
    : native_module_(std::move(native_module)),
      source_url_(std::move(source_url)) {}

CompiledWasmModule WasmModuleObject::GetCompiledModule() {
  auto* serial = new uint32_t(shim::host::shared_object_serial(
      *reinterpret_cast<const shim::Address*>(this)));
  std::shared_ptr<internal::wasm::NativeModule> module(
      reinterpret_cast<internal::wasm::NativeModule*>(serial),
      [](internal::wasm::NativeModule* key) {
        delete reinterpret_cast<uint32_t*>(key);
      });
  return CompiledWasmModule(std::move(module), "");
}

// The module may still be on its way from the isolate that sent it; waits
// for it when the thread can (no JavaScript running).
MaybeLocal<WasmModuleObject> WasmModuleObject::FromCompiledModule(
    Isolate* isolate, const CompiledWasmModule& compiled_module) {
  uint32_t serial = SerialOf(compiled_module.native_module_);
  if (!shim::host::shared_object_has(serial) &&
      shim::host::microtask_can_suspend())
    shim::host::shared_object_wait(serial);
  return Utils::ToMaybeLocal<WasmModuleObject>(
      shim::host::shared_object_get(serial));
}

// The memory's buffer (a SharedArrayBuffer for a shared memory).
Local<ArrayBuffer> WasmMemoryObject::Buffer() {
  return Utils::ToLocal<ArrayBuffer>(shim::host::wasm_memory_buffer(
      *reinterpret_cast<const shim::Address*>(this)));
}

namespace platform::tracing {

TracingController::TracingController() = default;
TracingController::~TracingController() = default;

}  // namespace platform::tracing

}  // namespace v8
