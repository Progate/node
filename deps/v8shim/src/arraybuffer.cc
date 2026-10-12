// ArrayBuffers and their backing stores.
//
// A JavaScript ArrayBuffer cannot point into WebAssembly memory, so each one
// that C++ touches has a mirror: a backing store in wasm memory that the host
// keeps in sync with the ArrayBuffer (see the host side, v8shim/buffers.ts).
// The host refers to a backing store through a holder, a heap-allocated
// std::shared_ptr<BackingStore>.

#include <stdlib.h>
#include <string.h>

#include <memory>
#include <new>

#include "src/shim.h"
#include "src/state.h"

namespace v8 {

using shim::Address;
namespace host = shim::host;

namespace {

Address Self(const void* self) {
  return *reinterpret_cast<const Address*>(self);
}

// What a BackingStore is. v8::BackingStore has no data of its own and a
// private constructor, so this record is allocated and used through it.
struct StoreData {
  void* data;
  size_t length;
  bool shared;
  // Stands for a SharedArrayBuffer of some isolate (v8shim_store_token).
  bool token = false;
  BackingStore::DeleterCallback deleter;
  void* deleter_data;
};

StoreData* DataOf(const BackingStore* store) {
  return reinterpret_cast<StoreData*>(const_cast<BackingStore*>(store));
}

BackingStore* NewStore(void* data, size_t length, bool shared,
                       BackingStore::DeleterCallback deleter,
                       void* deleter_data) {
  void* memory = ::operator new(sizeof(StoreData));
  new (memory) StoreData{data, length, shared, false, deleter, deleter_data};
  return reinterpret_cast<BackingStore*>(memory);
}

void FreeWithAllocator(void* data, size_t length, void* deleter_data) {
  static_cast<ArrayBuffer::Allocator*>(deleter_data)->Free(data, length);
}

void FreeWithLibc(void* data, size_t length, void* deleter_data) { free(data); }

// Memory from the isolate's allocator when there is one (Node accounts for
// it), otherwise from libc.
BackingStore* AllocateStore(size_t length, BackingStoreInitializationMode mode,
                            bool shared) {
  ArrayBuffer::Allocator* allocator = shim::GetState().array_buffer_allocator;
  size_t size = length > 0 ? length : 1;
  if (allocator != nullptr) {
    void* data = mode == BackingStoreInitializationMode::kZeroInitialized
                     ? allocator->Allocate(size)
                     : allocator->AllocateUninitialized(size);
    if (data == nullptr) return nullptr;
    return NewStore(data, length, shared, FreeWithAllocator, allocator);
  }
  void* data = calloc(1, size);
  if (data == nullptr) return nullptr;
  return NewStore(data, length, shared, FreeWithLibc, nullptr);
}

using Holder = std::shared_ptr<BackingStore>;

Holder* NewHolder(std::shared_ptr<BackingStore> store) {
  return new Holder(std::move(store));
}

// A JavaScript ArrayBuffer for a backing store: the host copies the current
// contents and keeps the store as the buffer's mirror.
// `aliased`: made empty by size (ArrayBuffer::New(isolate, length)), the way
// node makes the buffers it shares with JS; one made from a backing store
// hands over contents C++ already wrote.
// `shared` is 2 for a token (see shared_buffer_token).
Address ArrayBufferFor(std::shared_ptr<BackingStore> store, bool shared,
                       bool aliased) {
  StoreData* data = DataOf(store.get());
  int32_t kind = data->token ? 2 : shared ? 1 : 0;
  return host::array_buffer_new(NewHolder(std::move(store)),
                                reinterpret_cast<uintptr_t>(data->data),
                                static_cast<uint32_t>(data->length), kind,
                                aliased);
}

Holder* Pin(Address buffer) {
  return reinterpret_cast<Holder*>(host::array_buffer_pin(buffer));
}

class DefaultAllocator final : public ArrayBuffer::Allocator {
 public:
  void* Allocate(size_t length) override { return calloc(1, length ? length : 1); }
  void* AllocateUninitialized(size_t length) override {
    return malloc(length ? length : 1);
  }
  void Free(void* data, size_t length) override { free(data); }
};

}  // namespace

// The host asks for a mirror of an ArrayBuffer that JavaScript created.
extern "C" __attribute__((export_name("v8shim_store_new"))) Holder*
v8shim_store_new(uint32_t length, int32_t shared) {
  void* data = calloc(1, length > 0 ? length : 1);
  if (data == nullptr) shim::Fatal("out of memory for an ArrayBuffer (%u)", length);
  return NewHolder(std::shared_ptr<BackingStore>(
      NewStore(data, length, shared != 0, FreeWithLibc, nullptr)));
}

// Memory the host stages a guest's WASI call in (src/v8shim/guest-wasi.ts in
// @progate/browser-v8): the call goes to this program's own syscalls, whose
// pointers are into this memory. One per thread, grown as needed.
extern "C" __attribute__((export_name("v8shim_scratch"))) void* v8shim_scratch(
    uint32_t size) {
  static thread_local void* scratch = nullptr;
  static thread_local uint32_t capacity = 0;
  if (size > capacity) {
    void* grown = realloc(scratch, size);
    if (grown == nullptr) shim::Fatal("out of memory for a WASI call (%u)", size);
    scratch = grown;
    capacity = size;
  }
  return scratch;
}

// What the hosts of this program's threads share about SharedArrayBuffers:
// the name of the channel they pass them on (a number the first of them
// picks) and the last serial number of a token. The same for every thread
// because the memory is shared.
extern "C" __attribute__((export_name("v8shim_shared_channel"))) uint32_t*
v8shim_shared_channel() {
  alignas(8) static uint32_t channel[2] = {0, 0};
  return channel;
}

// The host asks for a token for a SharedArrayBuffer (see shared_buffer_token):
// a store whose data holds the token's serial number, which names the buffer
// between the hosts.
extern "C" __attribute__((export_name("v8shim_store_token"))) Holder*
v8shim_store_token(uint32_t length, uint32_t serial) {
  auto* key = static_cast<uint32_t*>(malloc(sizeof(uint32_t)));
  if (key == nullptr) shim::Fatal("out of memory for a SharedArrayBuffer token");
  *key = serial;
  BackingStore* store = NewStore(key, length, true, FreeWithLibc, nullptr);
  DataOf(store)->token = true;
  return NewHolder(std::shared_ptr<BackingStore>(store));
}

extern "C" __attribute__((export_name("v8shim_store_data"))) uintptr_t
v8shim_store_data(Holder* holder) {
  return reinterpret_cast<uintptr_t>(DataOf(holder->get())->data);
}

// The ArrayBuffer was collected; C++ may still hold the store.
extern "C" __attribute__((export_name("v8shim_store_release"))) void
v8shim_store_release(Holder* holder) {
  delete holder;
}

// --- BackingStore ----------------------------------------------------------------

BackingStore::~BackingStore() {
  StoreData* data = DataOf(this);
  if (data->deleter != nullptr)
    data->deleter(data->data, data->length, data->deleter_data);
}

void* BackingStore::Data() const { return DataOf(this)->data; }
size_t BackingStore::ByteLength() const { return DataOf(this)->length; }
size_t BackingStore::MaxByteLength() const { return DataOf(this)->length; }
bool BackingStore::IsShared() const { return DataOf(this)->shared; }
bool BackingStore::IsImmutable() const { return false; }
bool BackingStore::IsResizableByUserJavaScript() const { return false; }

void BackingStore::EmptyDeleter(void* data, size_t length,
                                void* deleter_data) {}

// --- ArrayBuffer -------------------------------------------------------------------

ArrayBuffer::Allocator* ArrayBuffer::Allocator::NewDefaultAllocator() {
  return new DefaultAllocator();
}

void ArrayBuffer::CheckCast(Value* that) {}
void ArrayBufferView::CheckCast(Value* that) {}
void SharedArrayBuffer::CheckCast(Value* that) {}
void PrimitiveArray::CheckCast(Data* that) {}

std::unique_ptr<BackingStore> ArrayBuffer::NewBackingStore(
    Isolate* isolate, size_t byte_length, BackingStoreInitializationMode mode,
    BackingStoreOnFailureMode on_failure) {
  BackingStore* store = AllocateStore(byte_length, mode, false);
  if (store == nullptr && on_failure == BackingStoreOnFailureMode::kOutOfMemory)
    shim::Fatal("out of memory for an ArrayBuffer (%zu)", byte_length);
  return std::unique_ptr<BackingStore>(store);
}

std::unique_ptr<BackingStore> ArrayBuffer::NewBackingStore(
    void* data, size_t byte_length, BackingStore::DeleterCallback deleter,
    void* deleter_data) {
  return std::unique_ptr<BackingStore>(
      NewStore(data, byte_length, false, deleter, deleter_data));
}

Local<ArrayBuffer> ArrayBuffer::New(Isolate* isolate, size_t byte_length,
                                    BackingStoreInitializationMode mode) {
  std::shared_ptr<BackingStore> store(
      AllocateStore(byte_length, BackingStoreInitializationMode::kZeroInitialized,
                    false));
  if (!store) shim::Fatal("out of memory for an ArrayBuffer (%zu)", byte_length);
  return Utils::ToLocal<ArrayBuffer>(
      ArrayBufferFor(std::move(store), false, true));
}

Local<ArrayBuffer> ArrayBuffer::New(Isolate* isolate,
                                    std::shared_ptr<BackingStore> backing_store) {
  return Utils::ToLocal<ArrayBuffer>(
      ArrayBufferFor(std::move(backing_store), false, false));
}

size_t ArrayBuffer::ByteLength() const {
  return host::array_buffer_length(Self(this));
}

void* ArrayBuffer::Data() const {
  return DataOf(Pin(Self(this))->get())->data;
}

std::shared_ptr<BackingStore> ArrayBuffer::GetBackingStore() {
  return *Pin(Self(this));
}

bool ArrayBuffer::IsDetachable() const { return true; }

bool ArrayBuffer::WasDetached() const {
  return host::array_buffer_was_detached(Self(this)) != 0;
}

bool ArrayBuffer::IsImmutable() const { return false; }

Maybe<bool> ArrayBuffer::Detach(Local<Value> key) {
  host::array_buffer_detach(Self(this));
  return Just(true);
}

void ArrayBuffer::SetDetachKey(Local<Value> key) {}

size_t ArrayBuffer::CopyArrayBufferBytes(size_t source_offset, size_t length,
                                       Local<ArrayBuffer> destination,
                                       size_t destination_offset) const {
  return host::array_buffer_copy(Self(this), source_offset,
                                 Utils::Open(destination), destination_offset,
                                 length);
}

// --- SharedArrayBuffer --------------------------------------------------------------

Local<SharedArrayBuffer> SharedArrayBuffer::New(
    Isolate* isolate, size_t byte_length, BackingStoreInitializationMode mode) {
  std::shared_ptr<BackingStore> store(AllocateStore(
      byte_length, BackingStoreInitializationMode::kZeroInitialized, true));
  if (!store) shim::Fatal("out of memory for a SharedArrayBuffer (%zu)", byte_length);
  return Utils::ToLocal<SharedArrayBuffer>(
      ArrayBufferFor(std::move(store), true, true));
}

MaybeLocal<SharedArrayBuffer> SharedArrayBuffer::MaybeNew(
    Isolate* isolate, size_t byte_length, BackingStoreInitializationMode mode) {
  std::shared_ptr<BackingStore> store(AllocateStore(
      byte_length, BackingStoreInitializationMode::kZeroInitialized, true));
  if (!store) return MaybeLocal<SharedArrayBuffer>();
  return Utils::ToLocal<SharedArrayBuffer>(
      ArrayBufferFor(std::move(store), true, true));
}

Local<SharedArrayBuffer> SharedArrayBuffer::New(
    Isolate* isolate, std::shared_ptr<BackingStore> backing_store) {
  // A token from another isolate: its buffer may still be on its way here.
  StoreData* store = DataOf(backing_store.get());
  uintptr_t data = reinterpret_cast<uintptr_t>(store->data);
  if (store->token && !host::shared_buffer_has(data) &&
      host::microtask_can_suspend())
    host::shared_buffer_wait(data);
  return Utils::ToLocal<SharedArrayBuffer>(
      ArrayBufferFor(std::move(backing_store), true, false));
}

size_t SharedArrayBuffer::ByteLength() const {
  return host::array_buffer_length(Self(this));
}

void* SharedArrayBuffer::Data() const {
  return DataOf(Pin(Self(this))->get())->data;
}

// The store stands for the buffer to pass it to another isolate (node's
// messaging); Data() gives the contents.
std::shared_ptr<BackingStore> SharedArrayBuffer::GetBackingStore() {
  return *reinterpret_cast<Holder*>(host::shared_buffer_token(Self(this)));
}

size_t SharedArrayBuffer::CopyArrayBufferBytes(
    size_t source_offset, size_t length, Local<SharedArrayBuffer> destination,
    size_t destination_offset) const {
  return host::array_buffer_copy(Self(this), source_offset,
                                 Utils::Open(destination), destination_offset,
                                 length);
}

// --- Views ------------------------------------------------------------------------

Local<ArrayBuffer> ArrayBufferView::Buffer() {
  return Utils::ToLocal<ArrayBuffer>(host::view_buffer(Self(this)));
}

size_t ArrayBufferView::ByteOffset() {
  return host::view_number(Self(this), host::kViewByteOffset);
}

size_t ArrayBufferView::ByteLength() {
  return host::view_number(Self(this), host::kViewByteLength);
}

bool ArrayBufferView::HasBuffer() const { return true; }

// Copies straight from the view: no mirror is needed.
size_t ArrayBufferView::CopyContents(void* dest, size_t byte_length) {
  return host::view_copy(Self(this), dest, byte_length);
}

size_t TypedArray::Length() {
  return host::view_number(Self(this), host::kViewLength);
}

#define V8SHIM_TYPED_ARRAY(Type, kind)                                        \
  Local<Type> Type::New(Local<ArrayBuffer> buffer, size_t byte_offset,         \
                        size_t length) {                                       \
    return Utils::ToLocal<Type>(host::typed_array_new(                         \
        host::kind, Utils::Open(buffer), byte_offset, length));                \
  }                                                                            \
  Local<Type> Type::New(Local<SharedArrayBuffer> buffer, size_t byte_offset,   \
                        size_t length) {                                       \
    return Utils::ToLocal<Type>(host::typed_array_new(                         \
        host::kind, Utils::Open(buffer), byte_offset, length));                \
  }                                                                            \
  void Type::CheckCast(Value* that) {}
V8SHIM_TYPED_ARRAY(Uint8Array, kUint8Array)
V8SHIM_TYPED_ARRAY(Uint8ClampedArray, kUint8ClampedArray)
V8SHIM_TYPED_ARRAY(Int8Array, kInt8Array)
V8SHIM_TYPED_ARRAY(Uint16Array, kUint16Array)
V8SHIM_TYPED_ARRAY(Int16Array, kInt16Array)
V8SHIM_TYPED_ARRAY(Uint32Array, kUint32Array)
V8SHIM_TYPED_ARRAY(Int32Array, kInt32Array)
V8SHIM_TYPED_ARRAY(Float16Array, kFloat16Array)
V8SHIM_TYPED_ARRAY(Float32Array, kFloat32Array)
V8SHIM_TYPED_ARRAY(Float64Array, kFloat64Array)
V8SHIM_TYPED_ARRAY(BigInt64Array, kBigInt64Array)
V8SHIM_TYPED_ARRAY(BigUint64Array, kBigUint64Array)
V8SHIM_TYPED_ARRAY(DataView, kDataView)
#undef V8SHIM_TYPED_ARRAY

}  // namespace v8
