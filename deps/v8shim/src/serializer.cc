// ValueSerializer and ValueDeserializer. The host writes and reads V8's wire
// format (serializer.ts in @progate/browser-v8); this side keeps the delegate,
// which the host calls back for host objects and SharedArrayBuffer ids.

#include <stdlib.h>
#include <string.h>

#include "src/shim.h"

namespace v8 {

using shim::Address;
namespace host = shim::host;

namespace {

void ThrowError(const char* message) {
  Isolate* isolate = shim::TheIsolate();
  isolate->ThrowException(Exception::Error(
      String::NewFromUtf8(isolate, message).ToLocalChecked()));
}

Address Keep(Address value) {
  return value == 0 ? 0 : host::global_new(value);
}

}  // namespace

// --- ValueSerializer ------------------------------------------------------------

struct ValueSerializer::PrivateData {
  Isolate* isolate;
  Delegate* delegate;
  int32_t id;
};

namespace {

// What the serializer does when there is no delegate.
class DefaultSerializerDelegate final : public ValueSerializer::Delegate {
 public:
  void ThrowDataCloneError(Local<String> message) override {
    shim::TheIsolate()->ThrowException(Exception::Error(message));
  }
};

DefaultSerializerDelegate* DefaultDelegate() {
  static auto* delegate = new DefaultSerializerDelegate();
  return delegate;
}

void ThrowCloneError(ValueSerializer::Delegate* delegate,
                     const char* message) {
  Isolate* isolate = shim::TheIsolate();
  delegate->ThrowDataCloneError(
      String::NewFromUtf8(isolate, message).ToLocalChecked());
}

}  // namespace

bool ValueSerializer::Delegate::HasCustomHostObject(Isolate* isolate) {
  return false;
}

Maybe<bool> ValueSerializer::Delegate::IsHostObject(Isolate* isolate,
                                                    Local<Object> object) {
  return Just(object->InternalFieldCount() > 0);
}

Maybe<bool> ValueSerializer::Delegate::WriteHostObject(Isolate* isolate,
                                                       Local<Object> object) {
  ThrowCloneError(this, "#<Object> could not be cloned.");
  return Nothing<bool>();
}

Maybe<uint32_t> ValueSerializer::Delegate::GetSharedArrayBufferId(
    Isolate* isolate, Local<SharedArrayBuffer> shared_array_buffer) {
  ThrowCloneError(this, "#<SharedArrayBuffer> could not be cloned.");
  return Nothing<uint32_t>();
}

Maybe<uint32_t> ValueSerializer::Delegate::GetWasmModuleTransferId(
    Isolate* isolate, Local<WasmModuleObject> module) {
  return Nothing<uint32_t>();
}

bool ValueSerializer::Delegate::AdoptSharedValueConveyor(
    Isolate* isolate, SharedValueConveyor&& conveyor) {
  ThrowCloneError(this, "#<Object> could not be cloned.");
  return false;
}

void* ValueSerializer::Delegate::ReallocateBufferMemory(void* old_buffer,
                                                        size_t size,
                                                        size_t* actual_size) {
  *actual_size = size;
  return realloc(old_buffer, size);
}

void ValueSerializer::Delegate::FreeBufferMemory(void* buffer) { free(buffer); }

ValueSerializer::ValueSerializer(Isolate* isolate)
    : ValueSerializer(isolate, nullptr) {}

ValueSerializer::ValueSerializer(Isolate* isolate, Delegate* delegate)
    : private_(new PrivateData{isolate,
                               delegate != nullptr ? delegate
                                                   : DefaultDelegate(),
                               0}) {
  private_->id = host::serializer_new(private_->delegate);
}

ValueSerializer::~ValueSerializer() {
  host::serializer_delete(private_->id);
  delete private_;
}

void ValueSerializer::WriteHeader() {
  host::serializer_write_header(private_->id);
}

Maybe<bool> ValueSerializer::WriteValue(Local<Context> context,
                                        Local<Value> value) {
  int32_t result =
      host::serializer_write_value(private_->id, Utils::Open(value));
  if (result == 1) return Just(true);
  if (result < 0) {
    HandleScope scope(private_->isolate);
    private_->delegate->ThrowDataCloneError(Utils::ToLocal<String>(
        host::serializer_error(private_->id)));
  }
  return Nothing<bool>();
}

std::pair<uint8_t*, size_t> ValueSerializer::Release() {
  size_t size = host::serializer_size(private_->id);
  size_t actual = 0;
  auto* buffer = static_cast<uint8_t*>(
      private_->delegate->ReallocateBufferMemory(nullptr, size, &actual));
  if (buffer == nullptr && size > 0) shim::Fatal("ValueSerializer: out of memory");
  host::serializer_release(private_->id, buffer);
  return {buffer, size};
}

void ValueSerializer::TransferArrayBuffer(uint32_t transfer_id,
                                          Local<ArrayBuffer> array_buffer) {
  host::serializer_transfer(private_->id, transfer_id,
                            Utils::Open(array_buffer));
}

void ValueSerializer::SetTreatArrayBufferViewsAsHostObjects(bool mode) {
  host::serializer_treat_views(private_->id, mode ? 1 : 0);
}

void ValueSerializer::WriteUint32(uint32_t value) {
  host::serializer_write_uint32(private_->id, value);
}

void ValueSerializer::WriteUint64(uint64_t value) {
  host::serializer_write_uint64(private_->id, value);
}

void ValueSerializer::WriteDouble(double value) {
  host::serializer_write_double(private_->id, value);
}

void ValueSerializer::WriteRawBytes(const void* source, size_t length) {
  host::serializer_write_raw(private_->id, source,
                             static_cast<uint32_t>(length));
}

// --- Calls from the host for the serializer's delegate ------------------------------
// 1 or 0; -1 with the exception pending.

extern "C" __attribute__((export_name("v8shim_serializer_is_host_object")))
int32_t v8shim_serializer_is_host_object(ValueSerializer::Delegate* delegate,
                                         Address object) {
  Isolate* isolate = shim::TheIsolate();
  HandleScope scope(isolate);
  Local<Object> local = Utils::ToLocal<Object>(object);
  if (!delegate->HasCustomHostObject(isolate))
    return local->InternalFieldCount() > 0 ? 1 : 0;
  Maybe<bool> result = delegate->IsHostObject(isolate, local);
  if (result.IsNothing()) return -1;
  return result.FromJust() ? 1 : 0;
}

extern "C" __attribute__((export_name("v8shim_serializer_write_host_object")))
int32_t v8shim_serializer_write_host_object(ValueSerializer::Delegate* delegate,
                                            Address object) {
  Isolate* isolate = shim::TheIsolate();
  HandleScope scope(isolate);
  Maybe<bool> result =
      delegate->WriteHostObject(isolate, Utils::ToLocal<Object>(object));
  return result.IsJust() && result.FromJust() ? 1 : 0;
}

// The id, or -1 with the exception pending.
extern "C" __attribute__((export_name("v8shim_serializer_shared_id"))) double
v8shim_serializer_shared_id(ValueSerializer::Delegate* delegate, Address buffer) {
  Isolate* isolate = shim::TheIsolate();
  HandleScope scope(isolate);
  Maybe<uint32_t> id = delegate->GetSharedArrayBufferId(
      isolate, Utils::ToLocal<SharedArrayBuffer>(buffer));
  return id.IsJust() ? static_cast<double>(id.FromJust()) : -1;
}

// The transfer id of a WebAssembly.Module, or -1 with the exception pending.
extern "C" __attribute__((export_name("v8shim_serializer_wasm_id"))) double
v8shim_serializer_wasm_id(ValueSerializer::Delegate* delegate, Address module) {
  Isolate* isolate = shim::TheIsolate();
  HandleScope scope(isolate);
  Maybe<uint32_t> id = delegate->GetWasmModuleTransferId(
      isolate, Utils::ToLocal<WasmModuleObject>(module));
  if (id.IsNothing()) {
    if (!isolate->HasPendingException())
      ThrowError("#<Module> could not be cloned.");
    return -1;
  }
  return static_cast<double>(id.FromJust());
}

// --- ValueDeserializer ------------------------------------------------------------

struct ValueDeserializer::PrivateData {
  Isolate* isolate;
  Delegate* delegate;
  const uint8_t* data;
  int32_t id;
};

MaybeLocal<Object> ValueDeserializer::Delegate::ReadHostObject(
    Isolate* isolate) {
  ThrowError("Unable to deserialize cloned data.");
  return MaybeLocal<Object>();
}

MaybeLocal<WasmModuleObject> ValueDeserializer::Delegate::GetWasmModuleFromId(
    Isolate* isolate, uint32_t transfer_id) {
  ThrowError("Unable to deserialize cloned data.");
  return MaybeLocal<WasmModuleObject>();
}

MaybeLocal<SharedArrayBuffer>
ValueDeserializer::Delegate::GetSharedArrayBufferFromId(Isolate* isolate,
                                                        uint32_t clone_id) {
  ThrowError("Unable to deserialize cloned data.");
  return MaybeLocal<SharedArrayBuffer>();
}

const SharedValueConveyor* ValueDeserializer::Delegate::GetSharedValueConveyor(
    Isolate* isolate) {
  ThrowError("Unable to deserialize cloned data.");
  return nullptr;
}

ValueDeserializer::ValueDeserializer(Isolate* isolate, const uint8_t* data,
                                     size_t size)
    : ValueDeserializer(isolate, data, size, nullptr) {}

ValueDeserializer::ValueDeserializer(Isolate* isolate, const uint8_t* data,
                                     size_t size, Delegate* delegate)
    : private_(new PrivateData{isolate, delegate, data, 0}) {
  private_->id =
      host::deserializer_new(delegate, data, static_cast<uint32_t>(size));
}

ValueDeserializer::~ValueDeserializer() {
  host::deserializer_delete(private_->id);
  delete private_;
}

Maybe<bool> ValueDeserializer::ReadHeader(Local<Context> context) {
  if (host::deserializer_read_header(private_->id) == 0) return Nothing<bool>();
  return Just(true);
}

MaybeLocal<Value> ValueDeserializer::ReadValue(Local<Context> context) {
  return Utils::ToMaybeLocal<Value>(
      host::deserializer_read_value(private_->id));
}

void ValueDeserializer::TransferArrayBuffer(uint32_t transfer_id,
                                            Local<ArrayBuffer> array_buffer) {
  host::deserializer_transfer(private_->id, transfer_id,
                              Utils::Open(array_buffer));
}

void ValueDeserializer::TransferSharedArrayBuffer(
    uint32_t id, Local<SharedArrayBuffer> shared_array_buffer) {
  host::deserializer_transfer(private_->id, id,
                              Utils::Open(shared_array_buffer));
}

// The host reads every version up to its own.
void ValueDeserializer::SetSupportsLegacyWireFormat(
    bool supports_legacy_wire_format) {}

uint32_t ValueDeserializer::GetWireFormatVersion() const {
  return host::deserializer_version(private_->id);
}

bool ValueDeserializer::ReadUint32(uint32_t* value) {
  return host::deserializer_read_uint32(private_->id, value) != 0;
}

bool ValueDeserializer::ReadUint64(uint64_t* value) {
  return host::deserializer_read_uint64(private_->id, value) != 0;
}

bool ValueDeserializer::ReadDouble(double* value) {
  return host::deserializer_read_double(private_->id, value) != 0;
}

bool ValueDeserializer::ReadRawBytes(size_t length, const void** data) {
  int32_t offset =
      host::deserializer_read_raw(private_->id, static_cast<uint32_t>(length));
  if (offset < 0) return false;
  *data = private_->data + offset;
  return true;
}

// --- Calls from the host for the deserializer's delegate ----------------------------
// A global entry the host disposes of, or 0 with the exception pending.

extern "C" __attribute__((export_name("v8shim_deserializer_read_host_object")))
Address v8shim_deserializer_read_host_object(ValueDeserializer::Delegate* delegate) {
  Isolate* isolate = shim::TheIsolate();
  HandleScope scope(isolate);
  if (delegate == nullptr) {
    ThrowError("Unable to deserialize cloned data.");
    return 0;
  }
  Local<Object> object;
  if (!delegate->ReadHostObject(isolate).ToLocal(&object)) return 0;
  return Keep(Utils::Open(object));
}

extern "C" __attribute__((export_name("v8shim_deserializer_wasm")))
Address v8shim_deserializer_wasm(ValueDeserializer::Delegate* delegate,
                                 uint32_t id) {
  Isolate* isolate = shim::TheIsolate();
  HandleScope scope(isolate);
  if (delegate == nullptr) {
    ThrowError("Unable to deserialize cloned data.");
    return 0;
  }
  Local<WasmModuleObject> module;
  if (!delegate->GetWasmModuleFromId(isolate, id).ToLocal(&module)) return 0;
  return Keep(Utils::Open(module));
}

extern "C" __attribute__((export_name("v8shim_deserializer_shared")))
Address v8shim_deserializer_shared(ValueDeserializer::Delegate* delegate,
                                   uint32_t id) {
  Isolate* isolate = shim::TheIsolate();
  HandleScope scope(isolate);
  if (delegate == nullptr) {
    ThrowError("Unable to deserialize cloned data.");
    return 0;
  }
  Local<SharedArrayBuffer> buffer;
  if (!delegate->GetSharedArrayBufferFromId(isolate, id).ToLocal(&buffer))
    return 0;
  return Keep(Utils::Open(buffer));
}

}  // namespace v8
