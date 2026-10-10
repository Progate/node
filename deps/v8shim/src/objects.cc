// Objects, functions, arrays, collections, promises and the like. Each call
// is one operation on the host engine's value.

#include <functional>
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

// Host results: -1 threw (the exception is pending), otherwise 0 or 1.
Maybe<bool> ToMaybeBool(int32_t result) {
  if (result < 0) return Nothing<bool>();
  return Just(result != 0);
}

std::vector<Address> Addresses(int argc, Local<Value>* argv) {
  std::vector<Address> addresses(argc > 0 ? argc : 0);
  for (int i = 0; i < argc; i++) addresses[i] = Utils::Open(argv[i]);
  return addresses;
}

}  // namespace

// --- Casts ---------------------------------------------------------------------

#define V8SHIM_CHECK_CAST(Type, Argument) \
  void Type::CheckCast(Argument* that) {}
V8SHIM_CHECK_CAST(Object, Value)
V8SHIM_CHECK_CAST(Array, Value)
V8SHIM_CHECK_CAST(Function, Value)
V8SHIM_CHECK_CAST(Map, Value)
V8SHIM_CHECK_CAST(Promise, Value)
V8SHIM_CHECK_CAST(Promise::Resolver, Value)
V8SHIM_CHECK_CAST(Proxy, Value)
V8SHIM_CHECK_CAST(RegExp, Value)
V8SHIM_CHECK_CAST(FixedArray, Data)
#undef V8SHIM_CHECK_CAST

// --- Object --------------------------------------------------------------------

Local<Object> Object::New(Isolate* isolate) {
  return Utils::ToLocal<Object>(host::object_new(0));
}

Local<Object> Object::New(Isolate* isolate, Local<Value> prototype_or_null,
                          Local<Name>* names, Local<Value>* values,
                          size_t length) {
  Address object = host::object_new(Utils::Open(prototype_or_null));
  for (size_t i = 0; i < length; i++)
    host::object_create_data_property(object, Utils::Open(names[i]),
                                      Utils::Open(values[i]));
  return Utils::ToLocal<Object>(object);
}

Maybe<bool> Object::Set(Local<Context> context, Local<Value> key,
                        Local<Value> value) {
  return ToMaybeBool(
      host::object_set(Self(this), Utils::Open(key), Utils::Open(value)));
}

Maybe<bool> Object::Set(Local<Context> context, uint32_t index,
                        Local<Value> value) {
  return ToMaybeBool(
      host::object_set(Self(this), host::number_new(index), Utils::Open(value)));
}

MaybeLocal<Value> Object::Get(Local<Context> context, Local<Value> key) {
  return Utils::ToMaybeLocal<Value>(host::object_get(Self(this), Utils::Open(key)));
}

MaybeLocal<Value> Object::Get(Local<Context> context, uint32_t index) {
  return Utils::ToMaybeLocal<Value>(
      host::object_get(Self(this), host::number_new(index)));
}

Maybe<bool> Object::Has(Local<Context> context, Local<Value> key) {
  return ToMaybeBool(host::object_has(Self(this), Utils::Open(key)));
}

Maybe<bool> Object::Delete(Local<Context> context, Local<Value> key) {
  return ToMaybeBool(host::object_delete(Self(this), Utils::Open(key)));
}

Maybe<bool> Object::Delete(Local<Context> context, uint32_t index) {
  return ToMaybeBool(host::object_delete(Self(this), host::number_new(index)));
}

Maybe<bool> Object::HasOwnProperty(Local<Context> context, Local<Name> key) {
  return ToMaybeBool(host::object_has_own(Self(this), Utils::Open(key)));
}

Maybe<bool> Object::HasRealNamedProperty(Local<Context> context,
                                         Local<Name> key) {
  return ToMaybeBool(host::object_has(Self(this), Utils::Open(key)));
}

MaybeLocal<Value> Object::GetRealNamedProperty(Local<Context> context,
                                               Local<Name> key) {
  return Get(context, key);
}

Maybe<PropertyAttribute> Object::GetRealNamedPropertyAttributes(
    Local<Context> context, Local<Name> key) {
  int32_t attributes =
      host::object_property_attributes(Self(this), Utils::Open(key), 1);
  if (attributes < 0) return Nothing<PropertyAttribute>();
  return Just(static_cast<PropertyAttribute>(attributes));
}

Maybe<PropertyAttribute> Object::GetPropertyAttributes(Local<Context> context,
                                                       Local<Value> key) {
  int32_t attributes =
      host::object_property_attributes(Self(this), Utils::Open(key), 1);
  if (attributes < 0) return Nothing<PropertyAttribute>();
  return Just(static_cast<PropertyAttribute>(attributes));
}

Maybe<bool> Object::CreateDataProperty(Local<Context> context, Local<Name> key,
                                       Local<Value> value) {
  return ToMaybeBool(host::object_create_data_property(
      Self(this), Utils::Open(key), Utils::Open(value)));
}

Maybe<bool> Object::DefineOwnProperty(Local<Context> context, Local<Name> key,
                                      Local<Value> value,
                                      PropertyAttribute attributes) {
  return ToMaybeBool(host::object_define_own(
      Self(this), Utils::Open(key), Utils::Open(value), attributes));
}

MaybeLocal<Value> Object::GetOwnPropertyDescriptor(Local<Context> context,
                                                   Local<Name> key) {
  return Utils::ToMaybeLocal<Value>(
      host::object_own_descriptor(Self(this), Utils::Open(key)));
}

MaybeLocal<Array> Object::GetOwnPropertyNames(Local<Context> context) {
  return Utils::ToMaybeLocal<Array>(host::object_keys(
      Self(this), static_cast<int32_t>(KeyCollectionMode::kOwnOnly),
      ONLY_ENUMERABLE | SKIP_SYMBOLS,
      static_cast<int32_t>(IndexFilter::kIncludeIndices),
      static_cast<int32_t>(KeyConversionMode::kKeepNumbers)));
}

MaybeLocal<Array> Object::GetPropertyNames(Local<Context> context,
                                           KeyCollectionMode mode,
                                           PropertyFilter property_filter,
                                           IndexFilter index_filter,
                                           KeyConversionMode key_conversion) {
  return Utils::ToMaybeLocal<Array>(host::object_keys(
      Self(this), static_cast<int32_t>(mode), property_filter,
      static_cast<int32_t>(index_filter), static_cast<int32_t>(key_conversion)));
}

Local<Value> Object::GetPrototype() {
  return Utils::ToLocal<Value>(host::object_get_prototype(Self(this)));
}

Maybe<bool> Object::SetPrototype(Local<Context> context,
                                 Local<Value> prototype) {
  return ToMaybeBool(
      host::object_set_prototype(Self(this), Utils::Open(prototype)));
}

Local<String> Object::GetConstructorName() {
  return Utils::ToLocal<String>(host::object_constructor_name(Self(this)));
}

Maybe<bool> Object::SetIntegrityLevel(Local<Context> context,
                                      IntegrityLevel level) {
  return ToMaybeBool(
      host::object_set_integrity(Self(this), static_cast<int32_t>(level)));
}

bool Object::IsConstructor() const {
  return host::object_is_constructor(Self(this)) != 0;
}

// There is one context; everything was created in it.
MaybeLocal<Context> Object::GetCreationContext() {
  return shim::TheIsolate()->GetCurrentContext();
}

Local<Context> Object::GetCreationContextChecked() {
  return GetCreationContext().ToLocalChecked();
}

MaybeLocal<Array> Object::PreviewEntries(bool* is_key_value) {
  int32_t key_value = 0;
  Address entries = host::object_preview_entries(Self(this), &key_value);
  *is_key_value = key_value != 0;
  return Utils::ToMaybeLocal<Array>(entries);
}

// Privates are kept by the host, out of JavaScript's reach.
Maybe<bool> Object::SetPrivate(Local<Context> context, Local<Private> key,
                               Local<Value> value) {
  host::private_set(Self(this), Utils::Open(key), Utils::Open(value));
  return Just(true);
}

MaybeLocal<Value> Object::GetPrivate(Local<Context> context,
                                     Local<Private> key) {
  return Utils::ToLocal<Value>(host::private_get(Self(this), Utils::Open(key)));
}

Maybe<bool> Object::HasPrivate(Local<Context> context, Local<Private> key) {
  return Just(host::private_has(Self(this), Utils::Open(key)) != 0);
}

Maybe<bool> Object::DeletePrivate(Local<Context> context, Local<Private> key) {
  host::private_delete(Self(this), Utils::Open(key));
  return Just(true);
}

// Internal fields and wrapped C++ pointers live in the host's side tables.
int Object::InternalFieldCount() const {
  return host::internal_field_count(Self(this));
}

void Object::SetInternalField(int index, Local<Data> value) {
  host::internal_field_set(Self(this), index, Utils::Open(value));
}

Local<Data> Object::SlowGetInternalField(int index) {
  return Utils::ToLocal<Data>(host::internal_field_get(Self(this), index));
}

void Object::SetAlignedPointerInInternalField(int index, void* value,
                                              EmbedderDataTypeTag tag) {
  host::internal_pointer_set(Self(this), index,
                             reinterpret_cast<uintptr_t>(value));
}

void* Object::SlowGetAlignedPointerFromInternalField(int index,
                                                     EmbedderDataTypeTag tag) {
  return reinterpret_cast<void*>(host::internal_pointer_get(Self(this), index));
}

void Object::Wrap(Isolate* isolate, Address wrapper, CppHeapPointerTag tag,
                  void* wrappable) {
  host::object_wrap(wrapper, reinterpret_cast<uintptr_t>(wrappable));
}

void* Object::Unwrap(Isolate* isolate, Address wrapper,
                     CppHeapPointerTagRange tag_range) {
  return reinterpret_cast<void*>(host::object_unwrap(wrapper));
}

// --- Function ------------------------------------------------------------------

MaybeLocal<Value> Function::Call(Local<Context> context, Local<Value> receiver,
                                 int argc, Local<Value> argv[]) {
  std::vector<Address> args = Addresses(argc, argv);
  return Utils::ToMaybeLocal<Value>(host::function_call(
      Self(this), Utils::Open(receiver), argc, args.data()));
}

MaybeLocal<Object> Function::NewInstance(Local<Context> context, int argc,
                                         Local<Value> argv[]) const {
  std::vector<Address> args = Addresses(argc, argv);
  return Utils::ToMaybeLocal<Object>(
      host::function_construct(Self(this), argc, args.data()));
}

void Function::SetName(Local<String> name) {
  host::function_set_name(Self(this), Utils::Open(name));
}

// V8 gives undefined, not an empty handle, for a script without a source map.
ScriptOrigin Function::GetScriptOrigin() const {
  return ScriptOrigin(
      Utils::ToLocal<Value>(host::function_script_name(Self(this))), 0, 0,
      false, -1, Undefined(Isolate::GetCurrent()));
}

// --- Array -----------------------------------------------------------------------

Local<Array> Array::New(Isolate* isolate, int length) {
  return Utils::ToLocal<Array>(host::array_new(length < 0 ? 0 : length));
}

Local<Array> Array::New(Isolate* isolate, Local<Value>* elements,
                        size_t length) {
  std::vector<Address> values = Addresses(static_cast<int>(length), elements);
  return Utils::ToLocal<Array>(
      host::array_from(values.data(), static_cast<uint32_t>(length)));
}

MaybeLocal<Array> Array::New(
    Local<Context> context, size_t length,
    std::function<MaybeLocal<v8::Value>()> next_value_callback) {
  Local<Array> array = New(shim::TheIsolate(), static_cast<int>(length));
  for (size_t i = 0; i < length; i++) {
    Local<v8::Value> value;
    if (!next_value_callback().ToLocal(&value)) return MaybeLocal<Array>();
    if (array->Set(context, static_cast<uint32_t>(i), value).IsNothing())
      return MaybeLocal<Array>();
  }
  return array;
}

uint32_t Array::Length() const { return host::array_length(Self(this)); }

Maybe<void> Array::Iterate(Local<Context> context, IterationCallback callback,
                           void* callback_data) {
  uint32_t length = Length();
  for (uint32_t i = 0; i < length; i++) {
    HandleScope scope(shim::TheIsolate());
    Local<v8::Value> element;
    if (!Get(context, i).ToLocal(&element)) return Nothing<void>();
    switch (callback(i, element, callback_data)) {
      case CallbackResult::kException:
        return Nothing<void>();
      case CallbackResult::kBreak:
        return JustVoid();
      case CallbackResult::kContinue:
        break;
    }
  }
  return JustVoid();
}

// --- Map and Set ---------------------------------------------------------------

Local<Map> Map::New(Isolate* isolate) {
  return Utils::ToLocal<Map>(host::collection_new(host::kMap));
}

MaybeLocal<Map> Map::Set(Local<Context> context, Local<Value> key,
                         Local<Value> value) {
  if (host::collection_add(Self(this), Utils::Open(key), Utils::Open(value)) < 0)
    return MaybeLocal<Map>();
  return Utils::ToLocal<Map>(Self(this));
}

Local<v8::Set> v8::Set::New(Isolate* isolate) {
  return Utils::ToLocal<v8::Set>(host::collection_new(host::kSet));
}

MaybeLocal<v8::Set> v8::Set::Add(Local<Context> context, Local<Value> key) {
  if (host::collection_add(Self(this), Utils::Open(key), 0) < 0)
    return MaybeLocal<v8::Set>();
  return Utils::ToLocal<v8::Set>(Self(this));
}

// --- Promises --------------------------------------------------------------------

MaybeLocal<Promise::Resolver> Promise::Resolver::New(Local<Context> context) {
  return Utils::ToMaybeLocal<Promise::Resolver>(host::resolver_new());
}

Local<Promise> Promise::Resolver::GetPromise() {
  return Utils::ToLocal<Promise>(host::resolver_promise(Self(this)));
}

Maybe<bool> Promise::Resolver::Resolve(Local<Context> context,
                                       Local<Value> value) {
  host::resolver_settle(Self(this), Utils::Open(value), 0);
  return Just(true);
}

Maybe<bool> Promise::Resolver::Reject(Local<Context> context,
                                      Local<Value> value) {
  host::resolver_settle(Self(this), Utils::Open(value), 1);
  return Just(true);
}

MaybeLocal<Promise> Promise::Then(Local<Context> context,
                                  Local<Function> on_fulfilled,
                                  Local<Function> on_rejected) {
  return Utils::ToMaybeLocal<Promise>(host::promise_then(
      Self(this), Utils::Open(on_fulfilled), Utils::Open(on_rejected)));
}

Promise::PromiseState Promise::State() {
  return static_cast<PromiseState>(host::promise_state(Self(this)));
}

Local<Value> Promise::Result() {
  return Utils::ToLocal<Value>(host::promise_result(Self(this)));
}

void Promise::MarkAsHandled() { host::promise_mark_handled(Self(this)); }

void Promise::MarkAsSilent() { host::promise_mark_handled(Self(this)); }

// --- Proxy, RegExp, JSON ---------------------------------------------------------

Local<Value> Proxy::GetTarget() {
  return Utils::ToLocal<Value>(host::proxy_part(Self(this), 0));
}

Local<Value> Proxy::GetHandler() {
  return Utils::ToLocal<Value>(host::proxy_part(Self(this), 1));
}

MaybeLocal<RegExp> RegExp::New(Local<Context> context, Local<String> pattern,
                               Flags flags) {
  return Utils::ToMaybeLocal<RegExp>(
      host::regexp_new(Utils::Open(pattern), flags));
}

MaybeLocal<Object> RegExp::Exec(Local<Context> context, Local<String> subject) {
  return Utils::ToMaybeLocal<Object>(
      host::regexp_exec(Self(this), Utils::Open(subject)));
}

MaybeLocal<String> JSON::Stringify(Local<Context> context,
                                   Local<Value> json_object,
                                   Local<String> gap) {
  return Utils::ToMaybeLocal<String>(
      host::json_stringify(Utils::Open(json_object), Utils::Open(gap)));
}

// --- Fixed and primitive arrays (host-defined options, module requests) ------

int FixedArray::Length() const {
  return static_cast<int>(host::array_length(Self(this)));
}

Local<Data> FixedArray::Get(int i) const {
  return Utils::ToLocal<Data>(
      host::object_get(Self(this), host::number_new(i)));
}

Local<PrimitiveArray> PrimitiveArray::New(Isolate* isolate, int length) {
  return Utils::ToLocal<PrimitiveArray>(host::array_new(length));
}

int PrimitiveArray::Length() const {
  return static_cast<int>(host::array_length(Self(this)));
}

void PrimitiveArray::Set(Isolate* isolate, int index, Local<Primitive> item) {
  host::object_set(Self(this), host::number_new(index), Utils::Open(item));
}

Local<Primitive> PrimitiveArray::Get(Isolate* isolate, int index) {
  return Utils::ToLocal<Primitive>(
      host::object_get(Self(this), host::number_new(index)));
}

// --- Property descriptors ------------------------------------------------------

struct PropertyDescriptor::PrivateData {
  Address value = 0;  // Global entries, or 0.
  Address get = 0;
  Address set = 0;
  bool has_writable = false;
  bool writable = false;
  bool has_enumerable = false;
  bool enumerable = false;
  bool has_configurable = false;
  bool configurable = false;
};

namespace {

Address Keep(Local<Value> value) {
  return value.IsEmpty() ? 0 : host::global_new(Utils::Open(value));
}

}  // namespace

PropertyDescriptor::PropertyDescriptor() : private_(new PrivateData()) {}

PropertyDescriptor::PropertyDescriptor(Local<Value> value)
    : private_(new PrivateData()) {
  private_->value = Keep(value);
}

PropertyDescriptor::PropertyDescriptor(Local<Value> value, bool writable)
    : private_(new PrivateData()) {
  private_->value = Keep(value);
  private_->has_writable = true;
  private_->writable = writable;
}

PropertyDescriptor::PropertyDescriptor(Local<Value> get, Local<Value> set)
    : private_(new PrivateData()) {
  private_->get = Keep(get);
  private_->set = Keep(set);
}

PropertyDescriptor::~PropertyDescriptor() {
  for (Address global : {private_->value, private_->get, private_->set})
    if (global) host::global_dispose(global);
  delete private_;
}

Local<Value> PropertyDescriptor::value() const {
  return Utils::ToLocal<Value>(private_->value);
}
Local<Value> PropertyDescriptor::get() const {
  return Utils::ToLocal<Value>(private_->get);
}
Local<Value> PropertyDescriptor::set() const {
  return Utils::ToLocal<Value>(private_->set);
}
bool PropertyDescriptor::has_value() const { return private_->value != 0; }
bool PropertyDescriptor::has_get() const { return private_->get != 0; }
bool PropertyDescriptor::has_set() const { return private_->set != 0; }
bool PropertyDescriptor::writable() const { return private_->writable; }
bool PropertyDescriptor::has_writable() const { return private_->has_writable; }
bool PropertyDescriptor::enumerable() const { return private_->enumerable; }
bool PropertyDescriptor::has_enumerable() const {
  return private_->has_enumerable;
}
bool PropertyDescriptor::configurable() const { return private_->configurable; }
bool PropertyDescriptor::has_configurable() const {
  return private_->has_configurable;
}
void PropertyDescriptor::set_enumerable(bool enumerable) {
  private_->has_enumerable = true;
  private_->enumerable = enumerable;
}
void PropertyDescriptor::set_configurable(bool configurable) {
  private_->has_configurable = true;
  private_->configurable = configurable;
}

Maybe<bool> Object::DefineProperty(Local<Context> context, Local<Name> key,
                                   PropertyDescriptor& descriptor) {
  const PropertyDescriptor::PrivateData* d = descriptor.get_private();
  int32_t flags = (d->value ? host::kHasValue : 0) |
                  (d->get ? host::kHasGet : 0) | (d->set ? host::kHasSet : 0) |
                  (d->has_writable ? host::kHasWritable : 0) |
                  (d->writable ? host::kWritable : 0) |
                  (d->has_enumerable ? host::kHasEnumerable : 0) |
                  (d->enumerable ? host::kEnumerable : 0) |
                  (d->has_configurable ? host::kHasConfigurable : 0) |
                  (d->configurable ? host::kConfigurable : 0);
  return ToMaybeBool(host::object_define_descriptor(
      Self(this), Utils::Open(key), d->value, d->get, d->set, flags));
}

}  // namespace v8
