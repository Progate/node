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

namespace platform::tracing {

TracingController::TracingController() = default;
TracingController::~TracingController() = default;

}  // namespace platform::tracing

}  // namespace v8
