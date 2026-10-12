// Templates, and calls from JavaScript into the C++ callbacks they carry.
//
// A template is a record on the host side. When JavaScript calls a function
// made from a FunctionTemplate, the host calls v8shim_call_function, which
// lays out a FunctionCallbackInfo exactly as V8's API exit frame does, so the
// inline accessors of v8-function-callback.h read it unchanged. Accessors and
// interceptors (a Proxy on the host side) go through v8shim_call_property
// with a PropertyCallbackInfo.

#include <vector>

#include "src/shim.h"

namespace v8 {

using shim::Address;
using shim::I;
namespace host = shim::host;

namespace internal {

// Friends of the callback info classes: they know the frame layout.
class FunctionCallbackArguments {
 public:
  using Info = FunctionCallbackInfo<Value>;
  static constexpr int kNewTarget = Info::kNewTargetIndex;
  static constexpr int kArgc = Info::kArgcIndex;
  static constexpr int kFrameType = Info::kFrameTypeIndex;
  static constexpr int kIsolate = Info::kIsolateIndex;
  static constexpr int kReturnValue = Info::kReturnValueIndex;
  static constexpr int kContext = Info::kContextIndex;
  static constexpr int kTarget = Info::kTargetIndex;
  static constexpr int kReceiver = Info::kReceiverIndex;
  static constexpr int kFirstArgument = Info::kFirstJSArgumentIndex;
};

class PropertyCallbackArguments {
 public:
  using Info = PropertyCallbackInfo<Value>;
  static constexpr int kPropertyKey = Info::kPropertyKeyIndex;
  static constexpr int kFrameType = Info::kFrameTypeIndex;
  static constexpr int kIsolate = Info::kIsolateIndex;
  static constexpr int kReturnValue = Info::kReturnValueIndex;
  static constexpr int kCallbackInfo = Info::kCallbackInfoIndex;
  static constexpr int kHolder = Info::kHolderIndex;
  static constexpr int kShouldThrowOnError = Info::kShouldThrowOnErrorIndex;
  static constexpr int kValue = Info::kValueIndex;
  static constexpr int kLength = Info::kFullArgsLength;
};

bool ShouldThrowOnError(internal::Isolate* isolate) { return false; }

Address ShimCallbackInfoData(Address callback_info) {
  return host::template_data(callback_info);
}

}  // namespace internal

namespace {

Address Self(const void* self) {
  return *reinterpret_cast<const Address*>(self);
}

Address OrUndefined(Local<Value> value) {
  return value.IsEmpty() ? shim::Undefined() : Utils::Open(value);
}

using FCA = internal::FunctionCallbackArguments;
using PCA = internal::PropertyCallbackArguments;

}  // namespace

// --- Calls into C++ ------------------------------------------------------------

// JavaScript called a function made from a template. `frame` is the host's
// record of the call: it fills in the receiver, the arguments and new.target.
extern "C" __attribute__((export_name("v8shim_call_function"))) void
v8shim_call_function(FunctionCallback callback, Address target, int32_t argc,
                     int32_t is_construct, uint32_t frame) {
  // values[0] is new.target (index -1 of the info); the info starts at 1.
  std::vector<Address> values(1 + FCA::kFirstArgument + argc);
  Address* info = values.data() + 1;
  info[FCA::kArgc] = static_cast<Address>(argc);
  info[FCA::kFrameType] = I::IntToSmi(
      is_construct ? I::kFrameTypeApiConstructExit : I::kFrameTypeApiCallExit);
  info[FCA::kIsolate] = reinterpret_cast<Address>(shim::TheIsolate());
  info[FCA::kReturnValue] = shim::Undefined();
  info[FCA::kContext] = shim::Undefined();
  info[FCA::kTarget] = target;
  host::frame_enter(frame, &info[FCA::kReturnValue], &info[FCA::kReceiver],
                    &info[FCA::kFirstArgument], &info[FCA::kNewTarget]);
  callback(*reinterpret_cast<FunctionCallbackInfo<Value>*>(info));
  host::frame_leave(frame, info[FCA::kReturnValue]);
}

// What v8shim_call_property runs. The numbers are shared with the host.
enum PropertyCall : int32_t {
  kAccessorGetter,
  kAccessorSetter,
  kNamedGetter,
  kNamedSetter,
  kNamedQuery,
  kNamedDeleter,
  kNamedEnumerator,
  kNamedDescriptor,
  kIndexedGetter,
  kIndexedSetter,
  kIndexedQuery,
  kIndexedDeleter,
  kIndexedEnumerator,
  kIndexedDescriptor,
};

// Returns whether an interceptor intercepted (1) or not (0). The result is in
// the frame's return value.
extern "C" __attribute__((export_name("v8shim_call_property"))) int32_t
v8shim_call_property(PropertyCall call, void* callback, Address callback_info,
                     Address holder, Address key, Address value,
                     uint32_t frame) {
  Address args[PCA::kLength] = {};
  bool indexed = call >= kIndexedGetter;
  args[PCA::kPropertyKey] = key;
  args[PCA::kFrameType] =
      I::IntToSmi(indexed ? I::kFrameTypeApiIndexedAccessorExit
                          : I::kFrameTypeApiNamedAccessorExit);
  args[PCA::kIsolate] = reinterpret_cast<Address>(shim::TheIsolate());
  args[PCA::kReturnValue] = shim::Undefined();
  args[PCA::kCallbackInfo] = callback_info;
  args[PCA::kHolder] = holder;
  args[PCA::kShouldThrowOnError] = I::IntToSmi(I::kDontThrow);
  args[PCA::kValue] = value;
  host::frame_enter(frame, &args[PCA::kReturnValue], nullptr, nullptr, nullptr);

  // The handles the callbacks take are slots in a scope of their own.
  HandleScope scope(shim::TheIsolate());
  Local<Name> name = Utils::ToLocal<Name>(key);
  Local<Value> new_value = Utils::ToLocal<Value>(value);
  uint32_t index = shim::IsSmi(key) ? static_cast<uint32_t>(I::SmiValue(key)) : 0;
  const auto& value_info = *reinterpret_cast<PropertyCallbackInfo<Value>*>(args);
  const auto& boolean_info =
      *reinterpret_cast<PropertyCallbackInfo<Boolean>*>(args);
  const auto& void_info = *reinterpret_cast<PropertyCallbackInfo<void>*>(args);
  const auto& integer_info =
      *reinterpret_cast<PropertyCallbackInfo<Integer>*>(args);
  const auto& array_info = *reinterpret_cast<PropertyCallbackInfo<Array>*>(args);
  Intercepted intercepted = Intercepted::kYes;
  switch (call) {
    case kAccessorGetter:
      reinterpret_cast<AccessorNameGetterCallback>(callback)(name, value_info);
      break;
    case kAccessorSetter:
      reinterpret_cast<AccessorNameSetterCallback>(callback)(name, new_value,
                                                              void_info);
      break;
    case kNamedGetter:
      intercepted =
          reinterpret_cast<NamedPropertyGetterCallback>(callback)(name, value_info);
      break;
    case kNamedSetter:
      intercepted = reinterpret_cast<NamedPropertySetterCallback>(callback)(
          name, new_value, void_info);
      break;
    case kNamedQuery:
      intercepted =
          reinterpret_cast<NamedPropertyQueryCallback>(callback)(name, integer_info);
      break;
    case kNamedDeleter:
      intercepted = reinterpret_cast<NamedPropertyDeleterCallback>(callback)(
          name, boolean_info);
      break;
    case kNamedEnumerator:
    case kIndexedEnumerator:
      reinterpret_cast<NamedPropertyEnumeratorCallback>(callback)(array_info);
      break;
    case kNamedDescriptor:
      intercepted = reinterpret_cast<NamedPropertyDescriptorCallback>(callback)(
          name, value_info);
      break;
    case kIndexedGetter:
      intercepted = reinterpret_cast<IndexedPropertyGetterCallbackV2>(callback)(
          index, value_info);
      break;
    case kIndexedSetter:
      intercepted = reinterpret_cast<IndexedPropertySetterCallbackV2>(callback)(
          index, new_value, void_info);
      break;
    case kIndexedQuery:
      intercepted = reinterpret_cast<IndexedPropertyQueryCallbackV2>(callback)(
          index, integer_info);
      break;
    case kIndexedDeleter:
      intercepted = reinterpret_cast<IndexedPropertyDeleterCallbackV2>(callback)(
          index, boolean_info);
      break;
    case kIndexedDescriptor:
      intercepted = reinterpret_cast<IndexedPropertyDescriptorCallbackV2>(
          callback)(index, value_info);
      break;
  }
  host::frame_leave(frame, args[PCA::kReturnValue]);
  return intercepted == Intercepted::kYes ? 1 : 0;
}

namespace api_internal {

// FunctionCallbackInfo::Data(): the target slot holds the template.
Local<Value> GetFunctionTemplateData(Isolate* isolate, Local<Data> target) {
  return Utils::ToLocal<Value>(host::template_data(Utils::Open(target)));
}

}  // namespace api_internal

// --- Templates -------------------------------------------------------------------

void FunctionTemplate::CheckCast(Data* that) {}
void ObjectTemplate::CheckCast(Data* that) {}

Local<FunctionTemplate> FunctionTemplate::New(
    Isolate* isolate, FunctionCallback callback, Local<Value> data,
    Local<Signature> signature, int length, ConstructorBehavior behavior,
    SideEffectType side_effect_type, const CFunction* c_function,
    uint16_t instance_type, uint16_t allowed_receiver_instance_type_range_start,
    uint16_t allowed_receiver_instance_type_range_end) {
  return Utils::ToLocal<FunctionTemplate>(host::template_new_function(
      reinterpret_cast<uintptr_t>(callback), OrUndefined(data), length,
      behavior == ConstructorBehavior::kAllow ? 1 : 0));
}

// Fast API calls need machine code; the slow callback is all there is.
Local<FunctionTemplate> FunctionTemplate::NewWithCFunctionOverloads(
    Isolate* isolate, FunctionCallback callback, Local<Value> data,
    Local<Signature> signature, int length, ConstructorBehavior behavior,
    SideEffectType side_effect_type,
    const MemorySpan<const CFunction>& c_function_overloads) {
  return New(isolate, callback, data, signature, length, behavior,
             side_effect_type);
}

CFunction::CFunction(const void* address, const CFunctionInfo* type_info)
    : address_(address), type_info_(type_info) {}

MaybeLocal<Function> FunctionTemplate::GetFunction(Local<Context> context) {
  return Utils::ToMaybeLocal<Function>(host::template_function(Self(this)));
}

Local<ObjectTemplate> FunctionTemplate::InstanceTemplate() {
  return Utils::ToLocal<ObjectTemplate>(
      host::template_part(Self(this), host::kInstanceTemplate));
}

Local<ObjectTemplate> FunctionTemplate::PrototypeTemplate() {
  return Utils::ToLocal<ObjectTemplate>(
      host::template_part(Self(this), host::kPrototypeTemplate));
}

void FunctionTemplate::Inherit(Local<FunctionTemplate> parent) {
  host::template_inherit(Self(this), Utils::Open(parent));
}

void FunctionTemplate::SetClassName(Local<String> name) {
  host::template_set_class_name(Self(this), Utils::Open(name));
}

void FunctionTemplate::SetLength(int length) {
  host::template_set_length(Self(this), length);
}

void FunctionTemplate::ReadOnlyPrototype() {
  host::template_read_only_prototype(Self(this));
}

bool FunctionTemplate::HasInstance(Local<Value> object) {
  return host::template_has_instance(Self(this), Utils::Open(object)) != 0;
}

Local<Signature> Signature::New(Isolate* isolate,
                                Local<FunctionTemplate> receiver) {
  // Receiver checks are left to the callbacks.
  return Utils::ToLocal<Signature>(
      receiver.IsEmpty() ? shim::Undefined() : Utils::Open(receiver));
}

Local<ObjectTemplate> ObjectTemplate::New(Isolate* isolate,
                                          Local<FunctionTemplate> constructor) {
  return Utils::ToLocal<ObjectTemplate>(host::template_new_object(
      constructor.IsEmpty() ? 0 : Utils::Open(constructor)));
}

MaybeLocal<Object> ObjectTemplate::NewInstance(Local<Context> context) {
  return Utils::ToMaybeLocal<Object>(host::template_new_instance(Self(this)));
}

void ObjectTemplate::SetInternalFieldCount(int value) {
  host::template_set_internal_field_count(Self(this), value);
}

void ObjectTemplate::SetHandler(
    const NamedPropertyHandlerConfiguration& configuration) {
  uintptr_t callbacks[] = {
      reinterpret_cast<uintptr_t>(configuration.getter),
      reinterpret_cast<uintptr_t>(configuration.setter),
      reinterpret_cast<uintptr_t>(configuration.query),
      reinterpret_cast<uintptr_t>(configuration.deleter),
      reinterpret_cast<uintptr_t>(configuration.enumerator),
      reinterpret_cast<uintptr_t>(configuration.descriptor),
  };
  host::template_set_handler(Self(this), 0, callbacks,
                             OrUndefined(configuration.data),
                             static_cast<int32_t>(configuration.flags));
}

void ObjectTemplate::SetHandler(
    const IndexedPropertyHandlerConfiguration& configuration) {
  uintptr_t callbacks[] = {
      reinterpret_cast<uintptr_t>(configuration.getter),
      reinterpret_cast<uintptr_t>(configuration.setter),
      reinterpret_cast<uintptr_t>(configuration.query),
      reinterpret_cast<uintptr_t>(configuration.deleter),
      reinterpret_cast<uintptr_t>(configuration.enumerator),
      reinterpret_cast<uintptr_t>(configuration.descriptor),
  };
  host::template_set_handler(Self(this), 1, callbacks,
                             OrUndefined(configuration.data),
                             static_cast<int32_t>(configuration.flags));
}

void Template::Set(Local<Name> name, Local<Data> value,
                   PropertyAttribute attributes) {
  host::template_set(Self(this), Utils::Open(name), Utils::Open(value),
                     attributes);
}

void Template::SetAccessorProperty(Local<Name> name,
                                   Local<FunctionTemplate> getter,
                                   Local<FunctionTemplate> setter,
                                   PropertyAttribute attribute) {
  host::template_set_accessor_property(
      Self(this), Utils::Open(name), Utils::Open(getter), Utils::Open(setter),
      attribute);
}

void Template::SetNativeDataProperty(Local<Name> name,
                                     AccessorNameGetterCallback getter,
                                     AccessorNameSetterCallback setter,
                                     Local<Value> data,
                                     PropertyAttribute attribute,
                                     SideEffectType getter_side_effect_type,
                                     SideEffectType setter_side_effect_type) {
  host::template_set_native_property(
      Self(this), Utils::Open(name), reinterpret_cast<uintptr_t>(getter),
      reinterpret_cast<uintptr_t>(setter), OrUndefined(data), attribute, 0);
}

void Template::SetLazyDataProperty(Local<Name> name,
                                   AccessorNameGetterCallback getter,
                                   Local<Value> data,
                                   PropertyAttribute attribute,
                                   SideEffectType getter_side_effect_type,
                                   SideEffectType setter_side_effect_type) {
  host::template_set_native_property(Self(this), Utils::Open(name),
                                     reinterpret_cast<uintptr_t>(getter), 0,
                                     OrUndefined(data), attribute, 1);
}

// The same on an object, without a template.
Maybe<bool> Object::SetNativeDataProperty(
    Local<Context> context, Local<Name> name, AccessorNameGetterCallback getter,
    AccessorNameSetterCallback setter, Local<Value> data,
    PropertyAttribute attributes, SideEffectType getter_side_effect_type,
    SideEffectType setter_side_effect_type) {
  host::object_set_native_property(
      Self(this), Utils::Open(name), reinterpret_cast<uintptr_t>(getter),
      reinterpret_cast<uintptr_t>(setter), OrUndefined(data), attributes, 0);
  return Just(true);
}

Maybe<bool> Object::SetLazyDataProperty(
    Local<Context> context, Local<Name> name, AccessorNameGetterCallback getter,
    Local<Value> data, PropertyAttribute attributes,
    SideEffectType getter_side_effect_type,
    SideEffectType setter_side_effect_type) {
  host::object_set_native_property(Self(this), Utils::Open(name),
                                   reinterpret_cast<uintptr_t>(getter), 0,
                                   OrUndefined(data), attributes, 1);
  return Just(true);
}

// A function without a template, e.g. for a one-off callback.
MaybeLocal<Function> Function::New(Local<Context> context,
                                   FunctionCallback callback, Local<Data> data,
                                   int length, ConstructorBehavior behavior,
                                   SideEffectType side_effect_type) {
  Address tmpl = host::template_new_function(
      reinterpret_cast<uintptr_t>(callback),
      data.IsEmpty() ? shim::Undefined() : Utils::Open(data), length,
      behavior == ConstructorBehavior::kAllow ? 1 : 0);
  return Utils::ToMaybeLocal<Function>(host::template_function(tmpl));
}

// --- DictionaryTemplate -------------------------------------------------------
// The template is the list of property names; an instance is a plain object
// with the values that are given.

Local<DictionaryTemplate> DictionaryTemplate::New(
    Isolate* isolate, MemorySpan<const std::string_view> names) {
  std::vector<Address> keys;
  keys.reserve(names.size());
  for (std::string_view name : names)
    keys.push_back(host::string_new(host::kUtf8, name.data(),
                                    static_cast<uint32_t>(name.size())));
  return Utils::ToLocal<DictionaryTemplate>(
      host::array_from(keys.data(), static_cast<uint32_t>(keys.size())));
}

Local<Object> DictionaryTemplate::NewInstance(
    Local<Context> context, MemorySpan<MaybeLocal<Value>> property_values) {
  Address names = Utils::Open(this);
  Address object = host::object_new(0);
  for (size_t i = 0; i < property_values.size(); i++) {
    Local<Value> value;
    if (!property_values[i].ToLocal(&value)) continue;
    Address key = host::object_get(names, I::IntToSmi(static_cast<int>(i)));
    host::object_set(object, key, Utils::Open(value));
  }
  return Utils::ToLocal<Object>(object);
}

}  // namespace v8
