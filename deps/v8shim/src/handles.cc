// Handles: HandleScope blocks, persistent (global) handles and the parts of
// v8-internal.h that read heap objects.

#include <stddef.h>
#include <stdlib.h>

#include <vector>

#include "src/shim.h"

namespace v8 {

using shim::Address;
using I = internal::Internals;

namespace {

// HandleScope slots live in blocks, as in V8.
constexpr size_t kBlockSize = 1022;

std::vector<Address*>& Blocks() {
  static auto* blocks = new std::vector<Address*>();
  return *blocks;
}

// The host table mark of every open HandleScope, innermost last.
std::vector<uint32_t>& Marks() {
  static auto* marks = new std::vector<uint32_t>();
  return *marks;
}

// A persistent handle points at a node: the value, then V8's class id and
// flag bytes (Internals::kNodeClassIdOffset / kNodeFlagsOffset), then what a
// weak handle needs.
struct Node {
  Address value;
  uint16_t class_id;
  uint8_t padding;
  uint8_t flags;
  void* parameter;
  WeakCallbackInfo<void>::Callback callback;
  // For weak handles without a callback: the handle to clear.
  Address** location_addr;
};
static_assert(offsetof(Node, class_id) == I::kNodeClassIdOffset);
static_assert(offsetof(Node, flags) == I::kNodeFlagsOffset);

Node* NodeOf(Address* slot) { return reinterpret_cast<Node*>(slot); }

}  // namespace

// --- HandleScope -------------------------------------------------------------

Address* HandleScope::Extend(Isolate* isolate) {
  internal::HandleScopeData* data = I::GetHandleScopeData(isolate);
  auto* block = static_cast<Address*>(malloc(kBlockSize * sizeof(Address)));
  V8SHIM_CHECK(block != nullptr);
  Blocks().push_back(block);
  data->next = block;
  data->limit = block + kBlockSize;
  return block;
}

void HandleScope::DeleteExtensions(Isolate* isolate) {
  // Free the blocks the closed scope added: those that do not hold the limit
  // that is current again.
  internal::HandleScopeData* data = I::GetHandleScopeData(isolate);
  auto& blocks = Blocks();
  while (!blocks.empty()) {
    Address* block = blocks.back();
    if (data->limit != nullptr && data->limit > block &&
        data->limit <= block + kBlockSize)
      break;
    blocks.pop_back();
    free(block);
  }
}

int HandleScope::NumberOfHandles(Isolate* isolate) {
  return static_cast<int>(Blocks().size() * kBlockSize);
}

Address* HandleScope::CreateHandleForCurrentIsolate(Address value) {
  return CreateHandle(shim::TheIsolate(), value);
}

// With V8_ENABLE_CHECKS, the inline HandleScope reports opening and closing.
// That is where the host table is cut back.
void HandleScope::DoInitializeAsserts(Isolate* isolate) {
  Marks().push_back(shim::host::scope_enter());
}

void HandleScope::AssertScopeLevelsMatch() {}

void HandleScope::DoCloseScopeAsserts(int before, Address* limit,
                                      internal::HandleScopeData* current) {
  V8SHIM_CHECK(!Marks().empty());
  shim::host::scope_exit(Marks().back());
  Marks().pop_back();
}

// An escapable scope reserves a slot, and a table entry, in the enclosing
// scope before it opens; Escape() puts the value there.
EscapableHandleScopeBase::EscapableHandleScopeBase(Isolate* isolate) {
  Address reserved = shim::host::scope_push(shim::Undefined());
  escape_slot_ = CreateHandle(isolate, reserved);
  Initialize(isolate);
}

Address* EscapableHandleScopeBase::EscapeSlot(Address* escape_value) {
  V8SHIM_CHECK(escape_slot_ != nullptr);
  Address value = escape_value == nullptr ? shim::Undefined() : *escape_value;
  if (shim::IsSmi(value))
    *escape_slot_ = value;
  else
    shim::host::table_set(*escape_slot_, value);
  Address* slot = escape_slot_;
  escape_slot_ = nullptr;
  return slot;
}

SealHandleScope::SealHandleScope(Isolate* isolate)
    : i_isolate_(reinterpret_cast<internal::Isolate*>(isolate)) {}

SealHandleScope::~SealHandleScope() = default;

// --- Persistent handles -------------------------------------------------------

namespace api_internal {

Address* GlobalizeReference(internal::Isolate* isolate, Address value) {
  auto* node = static_cast<Node*>(calloc(1, sizeof(Node)));
  V8SHIM_CHECK(node != nullptr);
  node->value = shim::host::global_new(value);
  return &node->value;
}

Address* CopyGlobalReference(Address* from) {
  return GlobalizeReference(nullptr, *from);
}

// Nodes do not move; the handle just points at the same node.
void MoveGlobalReference(Address** from, Address** to) {
  Node* node = NodeOf(*to);
  if (node->location_addr == from) node->location_addr = to;
}

void DisposeGlobal(Address* location) {
  Node* node = NodeOf(location);
  shim::host::global_dispose(node->value);
  free(node);
}

void MakeWeak(Address* location, void* parameter,
              WeakCallbackInfo<void>::Callback weak_callback,
              WeakCallbackType type) {
  Node* node = NodeOf(location);
  node->parameter = parameter;
  node->callback = weak_callback;
  I::UpdateNodeState(location, I::kNodeStateIsWeakValue);
  shim::host::global_make_weak(node->value, location);
}

void MakeWeak(Address** location_addr) {
  // Weak without a callback: the handle becomes empty once the value dies.
  Node* node = NodeOf(*location_addr);
  node->callback = nullptr;
  node->location_addr = location_addr;
  I::UpdateNodeState(*location_addr, I::kNodeStateIsWeakValue);
  shim::host::global_make_weak(node->value, *location_addr);
}

void* ClearWeak(Address* location) {
  Node* node = NodeOf(location);
  void* parameter = node->parameter;
  node->callback = nullptr;
  node->parameter = nullptr;
  node->location_addr = nullptr;
  I::UpdateNodeState(location, 0);
  shim::host::global_clear_weak(node->value);
  return parameter;
}

void AnnotateStrongRetainer(Address* location, const char* label) {}

// Eternal handles are globals that are never disposed.
Address* Eternalize(Isolate* isolate, Value* handle) {
  return GlobalizeReference(nullptr, Utils::Open(handle));
}

void FromJustIsNothing() { shim::Fatal("Maybe::FromJust() on Nothing"); }

void ToLocalEmpty() { shim::Fatal("MaybeLocal::ToLocalChecked() on empty"); }

void InternalFieldOutOfBounds(int index) {
  shim::Fatal("internal field %d out of bounds", index);
}

}  // namespace api_internal

// --- What v8-internal.h would read from the heap ----------------------------

namespace internal {

int ShimGetInstanceType(Address obj) { return shim::host::instance_type(obj); }

Address ShimLoadMap(Address obj) { return shim::host::load_map(obj); }

int ShimGetOddballKind(Address obj) { return shim::host::oddball_kind(obj); }

void ShimUnsupportedRawField(Address obj, int offset) {
  shim::Fatal("read of a raw heap field (offset %d) of %#lx", offset,
              static_cast<unsigned long>(obj));
}

bool ShimSameValue(Address lhs, Address rhs) {
  if (lhs == rhs) return true;
  if (shim::IsSmi(lhs) || shim::IsSmi(rhs)) return false;
  return shim::host::same_value(lhs, rhs) != 0;
}

void ShimSetReturnValue(Address* slot, Address value) {
  shim::host::set_return_value(slot, value);
}

}  // namespace internal

// The host calls this once a weak global's value has been collected. It runs
// the first-pass callback and, if that asks for one, the second pass, as V8
// does for WeakCallbackType::kParameter. Internal fields are not passed.
extern "C" __attribute__((export_name("v8shim_weak_callback"))) void
v8shim_weak_callback(Address* location) {
  Node* node = NodeOf(location);
  if (node->callback == nullptr) {
    Address** location_addr = node->location_addr;
    api_internal::DisposeGlobal(location);
    if (location_addr != nullptr) *location_addr = nullptr;
    return;
  }
  Isolate* isolate = shim::TheIsolate();
  HandleScope scope(isolate);
  void* fields[kEmbedderFieldsInWeakCallback] = {};
  WeakCallbackInfo<void>::Callback second_pass = nullptr;
  WeakCallbackInfo<void> first(isolate, node->parameter, fields, &second_pass);
  node->callback(first);
  if (second_pass != nullptr) {
    WeakCallbackInfo<void> second(isolate, node->parameter, fields, nullptr);
    second_pass(second);
  }
}

}  // namespace v8
