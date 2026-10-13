// Values: type checks, conversions, numbers, strings, symbols, externals.

#include <stdlib.h>
#include <string.h>

#include <cmath>

#include "src/shim.h"

namespace v8 {

using shim::Address;
using shim::I;
namespace host = shim::host;

namespace {

Address Self(const void* self) {
  return *reinterpret_cast<const Address*>(self);
}

bool Is(const void* self, host::ValueKind kind) {
  return host::value_is(Self(self), kind) != 0;
}

}  // namespace

// --- Type checks ---------------------------------------------------------------

bool Data::IsValue() const { return !Is(this, host::kIsApiData); }
bool Data::IsModule() const { return Is(this, host::kIsModule); }
bool Data::IsPrivate() const { return Is(this, host::kIsPrivate); }

bool Value::FullIsUndefined() const { return Self(this) == shim::Undefined(); }
bool Value::FullIsNull() const { return Self(this) == shim::Null(); }
bool Value::FullIsTrue() const { return Self(this) == shim::True(); }
bool Value::FullIsFalse() const { return Self(this) == shim::False(); }
bool Value::FullIsString() const { return Is(this, host::kIsString); }

#define V8SHIM_VALUE_IS(Name, kind) \
  bool Value::Is##Name() const { return Is(this, host::kind); }
V8SHIM_VALUE_IS(ArgumentsObject, kIsArgumentsObject)
V8SHIM_VALUE_IS(Array, kIsArray)
V8SHIM_VALUE_IS(ArrayBuffer, kIsArrayBuffer)
V8SHIM_VALUE_IS(ArrayBufferView, kIsArrayBufferView)
V8SHIM_VALUE_IS(AsyncFunction, kIsAsyncFunction)
V8SHIM_VALUE_IS(BigInt, kIsBigInt)
V8SHIM_VALUE_IS(BigInt64Array, kIsBigInt64Array)
V8SHIM_VALUE_IS(BigIntObject, kIsBigIntObject)
V8SHIM_VALUE_IS(BigUint64Array, kIsBigUint64Array)
V8SHIM_VALUE_IS(Boolean, kIsBoolean)
V8SHIM_VALUE_IS(BooleanObject, kIsBooleanObject)
V8SHIM_VALUE_IS(DataView, kIsDataView)
V8SHIM_VALUE_IS(Date, kIsDate)
V8SHIM_VALUE_IS(External, kIsExternal)
V8SHIM_VALUE_IS(Float16Array, kIsFloat16Array)
V8SHIM_VALUE_IS(Float32Array, kIsFloat32Array)
V8SHIM_VALUE_IS(Float64Array, kIsFloat64Array)
V8SHIM_VALUE_IS(Function, kIsFunction)
V8SHIM_VALUE_IS(GeneratorFunction, kIsGeneratorFunction)
V8SHIM_VALUE_IS(GeneratorObject, kIsGeneratorObject)
V8SHIM_VALUE_IS(Int16Array, kIsInt16Array)
V8SHIM_VALUE_IS(Int32, kIsInt32)
V8SHIM_VALUE_IS(Int32Array, kIsInt32Array)
V8SHIM_VALUE_IS(Int8Array, kIsInt8Array)
V8SHIM_VALUE_IS(Map, kIsMap)
V8SHIM_VALUE_IS(MapIterator, kIsMapIterator)
V8SHIM_VALUE_IS(ModuleNamespaceObject, kIsModuleNamespaceObject)
V8SHIM_VALUE_IS(NativeError, kIsNativeError)
V8SHIM_VALUE_IS(Number, kIsNumber)
V8SHIM_VALUE_IS(NumberObject, kIsNumberObject)
V8SHIM_VALUE_IS(Object, kIsObject)
V8SHIM_VALUE_IS(Promise, kIsPromise)
V8SHIM_VALUE_IS(Proxy, kIsProxy)
V8SHIM_VALUE_IS(RegExp, kIsRegExp)
V8SHIM_VALUE_IS(Set, kIsSet)
V8SHIM_VALUE_IS(SetIterator, kIsSetIterator)
V8SHIM_VALUE_IS(SharedArrayBuffer, kIsSharedArrayBuffer)
V8SHIM_VALUE_IS(StringObject, kIsStringObject)
V8SHIM_VALUE_IS(Symbol, kIsSymbol)
V8SHIM_VALUE_IS(SymbolObject, kIsSymbolObject)
V8SHIM_VALUE_IS(TypedArray, kIsTypedArray)
V8SHIM_VALUE_IS(Uint16Array, kIsUint16Array)
V8SHIM_VALUE_IS(Uint32, kIsUint32)
V8SHIM_VALUE_IS(Uint32Array, kIsUint32Array)
V8SHIM_VALUE_IS(Uint8Array, kIsUint8Array)
V8SHIM_VALUE_IS(Uint8ClampedArray, kIsUint8ClampedArray)
V8SHIM_VALUE_IS(WasmMemoryObject, kIsWasmMemoryObject)
V8SHIM_VALUE_IS(WeakMap, kIsWeakMap)
V8SHIM_VALUE_IS(WeakSet, kIsWeakSet)
#undef V8SHIM_VALUE_IS

// The headers call these with V8_ENABLE_CHECKS before a cast. The host engine
// checks types itself, so there is nothing to verify.
#define V8SHIM_CHECK_CAST(Type) \
  void Type::CheckCast(Data* that) {}
V8SHIM_CHECK_CAST(BigInt)
V8SHIM_CHECK_CAST(Boolean)
V8SHIM_CHECK_CAST(External)
V8SHIM_CHECK_CAST(Int32)
V8SHIM_CHECK_CAST(Integer)
V8SHIM_CHECK_CAST(Name)
V8SHIM_CHECK_CAST(Number)
V8SHIM_CHECK_CAST(Private)
V8SHIM_CHECK_CAST(String)
V8SHIM_CHECK_CAST(Symbol)
V8SHIM_CHECK_CAST(Uint32)
V8SHIM_CHECK_CAST(Value)
#undef V8SHIM_CHECK_CAST

// --- Conversions -----------------------------------------------------------------

MaybeLocal<String> Value::ToString(Local<Context> context) const {
  return Utils::ToMaybeLocal<String>(host::value_to(Self(this), host::kToString));
}

MaybeLocal<String> Value::ToDetailString(Local<Context> context) const {
  return Utils::ToMaybeLocal<String>(
      host::value_to(Self(this), host::kToDetailString));
}

MaybeLocal<Object> Value::ToObject(Local<Context> context) const {
  return Utils::ToMaybeLocal<Object>(host::value_to(Self(this), host::kToObject));
}

Local<Boolean> Value::ToBoolean(Isolate* isolate) const {
  return Utils::ToLocal<Boolean>(shim::Boolean(BooleanValue(isolate)));
}

bool Value::BooleanValue(Isolate* isolate) const {
  return host::value_truthy(Self(this)) != 0;
}

Local<String> Value::TypeOf(Isolate* isolate) {
  return Utils::ToLocal<String>(host::value_to(Self(this), host::kTypeOf));
}

Maybe<double> Value::NumberValue(Local<Context> context) const {
  Address self = Self(this);
  if (shim::IsSmi(self)) return Just<double>(I::SmiValue(self));
  double value;
  if (!host::value_to_number(self, &value)) return Nothing<double>();
  return Just(value);
}

namespace {

// ECMAScript ToInt32 / ToUint32 from a Number.
uint32_t ModuloTwoTo32(double value) {
  if (!std::isfinite(value)) return 0;
  double truncated = std::trunc(value);
  double wrapped = std::fmod(truncated, 4294967296.0);
  if (wrapped < 0) wrapped += 4294967296.0;
  return static_cast<uint32_t>(wrapped);
}

}  // namespace

Maybe<int64_t> Value::IntegerValue(Local<Context> context) const {
  Maybe<double> number = NumberValue(context);
  if (number.IsNothing()) return Nothing<int64_t>();
  double value = number.FromJust();
  if (std::isnan(value)) return Just<int64_t>(0);
  if (value >= 9223372036854775807.0) return Just(INT64_MAX);
  if (value <= -9223372036854775808.0) return Just(INT64_MIN);
  return Just(static_cast<int64_t>(value));
}

Maybe<int32_t> Value::Int32Value(Local<Context> context) const {
  Address self = Self(this);
  if (shim::IsSmi(self)) return Just<int32_t>(I::SmiValue(self));
  Maybe<double> number = NumberValue(context);
  if (number.IsNothing()) return Nothing<int32_t>();
  return Just(static_cast<int32_t>(ModuloTwoTo32(number.FromJust())));
}

Maybe<uint32_t> Value::Uint32Value(Local<Context> context) const {
  Address self = Self(this);
  if (shim::IsSmi(self)) return Just(static_cast<uint32_t>(I::SmiValue(self)));
  Maybe<double> number = NumberValue(context);
  if (number.IsNothing()) return Nothing<uint32_t>();
  return Just(ModuloTwoTo32(number.FromJust()));
}

Maybe<bool> Value::Equals(Local<Context> context, Local<Value> that) const {
  int32_t result =
      host::value_equals(Self(this), Utils::Open(that), host::kLooseEquals);
  if (result < 0) return Nothing<bool>();
  return Just(result != 0);
}

bool Value::StrictEquals(Local<Value> that) const {
  return host::value_equals(Self(this), Utils::Open(that),
                            host::kStrictEquals) != 0;
}

bool Value::SameValue(Local<Value> that) const {
  return host::value_equals(Self(this), Utils::Open(that), host::kSameValue) !=
         0;
}

// --- Numbers -------------------------------------------------------------------

Local<Number> Number::New(Isolate* isolate, double value) {
  return Utils::ToLocal<Number>(host::number_new(value));
}

// Through double: the header's integer overloads of New() call these.
Local<Number> Number::NewFromInt32(Isolate* isolate, int32_t value) {
  return New(isolate, static_cast<double>(value));
}

Local<Number> Number::NewFromUint32(Isolate* isolate, uint32_t value) {
  return New(isolate, static_cast<double>(value));
}

double Number::Value() const {
  Address self = Self(this);
  if (shim::IsSmi(self)) return I::SmiValue(self);
  double value = 0;
  host::value_to_number(self, &value);
  return value;
}

Local<Integer> Integer::New(Isolate* isolate, int32_t value) {
  return Utils::ToLocal<Integer>(host::number_new(value));
}

Local<Integer> Integer::NewFromUnsigned(Isolate* isolate, uint32_t value) {
  return Utils::ToLocal<Integer>(host::number_new(value));
}

int64_t Integer::Value() const {
  return static_cast<int64_t>(reinterpret_cast<const Number*>(this)->Value());
}

int32_t Int32::Value() const {
  return static_cast<int32_t>(reinterpret_cast<const Number*>(this)->Value());
}

uint32_t Uint32::Value() const {
  return static_cast<uint32_t>(reinterpret_cast<const Number*>(this)->Value());
}

bool Boolean::Value() const { return Self(this) == shim::True(); }

Local<BigInt> BigInt::New(Isolate* isolate, int64_t value) {
  return Utils::ToLocal<BigInt>(host::bigint_new(value, 0));
}

Local<BigInt> BigInt::NewFromUnsigned(Isolate* isolate, uint64_t value) {
  return Utils::ToLocal<BigInt>(host::bigint_new(static_cast<int64_t>(value), 1));
}

uint64_t BigInt::Uint64Value(bool* lossless) const {
  int32_t exact = 0;
  uint64_t value = static_cast<uint64_t>(host::bigint_value(Self(this), 1, &exact));
  if (lossless != nullptr) *lossless = exact != 0;
  return value;
}

int64_t BigInt::Int64Value(bool* lossless) const {
  int32_t exact = 0;
  int64_t value = host::bigint_value(Self(this), 0, &exact);
  if (lossless != nullptr) *lossless = exact != 0;
  return value;
}

// --- Strings -----------------------------------------------------------------------

namespace {

MaybeLocal<String> NewString(host::Encoding encoding, const void* data,
                             int length, size_t unit) {
  if (length < 0) {
    length = 0;
    if (unit == 1) {
      length = static_cast<int>(strlen(static_cast<const char*>(data)));
    } else {
      auto* units = static_cast<const uint16_t*>(data);
      while (units[length] != 0) length++;
    }
  }
  return Utils::ToMaybeLocal<String>(
      host::string_new(encoding, data, static_cast<uint32_t>(length)));
}

}  // namespace

MaybeLocal<String> String::NewFromUtf8(Isolate* isolate, const char* data,
                                       NewStringType type, int length) {
  return NewString(host::kUtf8, data, length, 1);
}

Local<String> String::NewFromUtf8Literal(Isolate* isolate, const char* literal,
                                         NewStringType type, int length) {
  return NewString(host::kUtf8, literal, length, 1).ToLocalChecked();
}

MaybeLocal<String> String::NewFromOneByte(Isolate* isolate,
                                          const uint8_t* data,
                                          NewStringType type, int length) {
  return NewString(host::kLatin1, data, length, 1);
}

MaybeLocal<String> String::NewFromTwoByte(Isolate* isolate,
                                          const uint16_t* data,
                                          NewStringType type, int length) {
  return NewString(host::kUtf16, data, length, 2);
}

// External strings are copied into the host engine and the resource is
// disposed right away; IsExternal*() then report false, as for any string
// that V8 internalized.
MaybeLocal<String> String::NewExternalOneByte(
    Isolate* isolate, ExternalOneByteStringResource* resource) {
  MaybeLocal<String> string = NewString(
      host::kLatin1, resource->data(), static_cast<int>(resource->length()), 1);
  resource->Dispose();
  return string;
}

MaybeLocal<String> String::NewExternalTwoByte(Isolate* isolate,
                                              ExternalStringResource* resource) {
  MaybeLocal<String> string = NewString(
      host::kUtf16, resource->data(), static_cast<int>(resource->length()), 2);
  resource->Dispose();
  return string;
}

bool String::IsExternalOneByte() const { return false; }
bool String::IsExternalTwoByte() const { return false; }

String::ExternalStringResource* String::GetExternalStringResourceSlow() const {
  return nullptr;
}

const String::ExternalOneByteStringResource*
String::GetExternalOneByteStringResource() const {
  return nullptr;
}

String::ExternalStringResourceBase* String::GetExternalStringResourceBaseSlow(
    String::Encoding* encoding_out) const {
  return nullptr;
}

void String::VerifyExternalStringResourceBase(ExternalStringResourceBase* v,
                                              Encoding encoding) const {}

void String::VerifyExternalStringResource(ExternalStringResource* val) const {}

int String::Length() const {
  return static_cast<int>(host::string_length(Self(this)));
}

bool String::IsOneByte() const { return host::string_is_one_byte(Self(this)) != 0; }

bool String::ContainsOnlyOneByte() const { return IsOneByte(); }

size_t String::Utf8LengthV2(Isolate* isolate) const {
  return host::string_utf8_length(Self(this));
}

void String::WriteV2(Isolate* isolate, uint32_t offset, uint32_t length,
                     uint16_t* buffer, int flags) const {
  host::string_write(Self(this), host::kUtf16, offset, length, buffer);
  if (flags & WriteFlags::kNullTerminate) buffer[length] = 0;
}

void String::WriteOneByteV2(Isolate* isolate, uint32_t offset,
                            uint32_t length, uint8_t* buffer,
                            int flags) const {
  host::string_write(Self(this), host::kLatin1, offset, length, buffer);
  if (flags & WriteFlags::kNullTerminate) buffer[length] = 0;
}

size_t String::WriteUtf8V2(Isolate* isolate, char* buffer, size_t capacity,
                           int flags,
                           size_t* processed_characters_return) const {
  uint32_t processed = 0;
  bool terminate = (flags & WriteFlags::kNullTerminate) != 0;
  size_t room = terminate ? (capacity > 0 ? capacity - 1 : 0) : capacity;
  size_t written =
      host::string_write_utf8(Self(this), buffer, room, &processed);
  if (terminate && capacity > 0) buffer[written++] = '\0';
  if (processed_characters_return != nullptr)
    *processed_characters_return = processed;
  return written;
}

Local<String> String::Concat(Isolate* isolate, Local<String> left,
                             Local<String> right) {
  return Utils::ToLocal<String>(
      host::string_concat(Utils::Open(left), Utils::Open(right)));
}

// A view copies the characters into memory it owns until it is destroyed.
String::ValueView::ValueView(Isolate* isolate, Local<String> str)
    : flat_str_(str) {
  Address self = Utils::Open(str);
  length_ = host::string_length(self);
  is_one_byte_ = host::string_is_one_byte(self) != 0;
  if (is_one_byte_) {
    auto* data = static_cast<uint8_t*>(malloc(length_ + 1));
    host::string_write(self, host::kLatin1, 0, length_, data);
    data8_ = data;
  } else {
    auto* data = static_cast<uint16_t*>(malloc((length_ + 1) * 2));
    host::string_write(self, host::kUtf16, 0, length_, data);
    data16_ = data;
  }
}

String::ValueView::~ValueView() {
  free(const_cast<uint8_t*>(data8_));
}

void String::ValueView::CheckOneByte(bool is_one_byte) const {}

// --- Names, symbols, privates ------------------------------------------------

int Name::GetIdentityHash() { return host::identity_hash(Self(this)); }

Local<Symbol> Symbol::New(Isolate* isolate, Local<String> description) {
  return Utils::ToLocal<Symbol>(host::symbol_new(Utils::Open(description)));
}

Local<Value> Symbol::Description(Isolate* isolate) const {
  return Utils::ToLocal<Value>(host::symbol_description(Self(this)));
}

Local<Symbol> Symbol::GetIterator(Isolate* isolate) {
  return Utils::ToLocal<Symbol>(host::symbol_well_known(host::kSymbolIterator));
}

Local<Symbol> Symbol::For(Isolate* isolate, Local<String> description) {
  return Utils::ToLocal<Symbol>(host::symbol_for(Utils::Open(description)));
}

Local<Symbol> Symbol::GetDispose(Isolate* isolate) {
  return Utils::ToLocal<Symbol>(host::symbol_well_known(host::kSymbolDispose));
}

Local<Symbol> Symbol::GetAsyncDispose(Isolate* isolate) {
  return Utils::ToLocal<Symbol>(
      host::symbol_well_known(host::kSymbolAsyncDispose));
}

Local<Symbol> Symbol::GetToStringTag(Isolate* isolate) {
  return Utils::ToLocal<Symbol>(
      host::symbol_well_known(host::kSymbolToStringTag));
}

// A private is a key the host keeps its own table for: properties stored
// under it are invisible to JavaScript.
Local<Private> Private::New(Isolate* isolate, Local<String> name) {
  return Utils::ToLocal<Private>(host::private_new(Utils::Open(name), 0));
}

Local<Private> Private::ForApi(Isolate* isolate, Local<String> name) {
  return Utils::ToLocal<Private>(host::private_new(Utils::Open(name), 1));
}

// --- Externals -------------------------------------------------------------------

Local<External> External::New(Isolate* isolate, void* value,
                              ExternalPointerTypeTag tag) {
  return Utils::ToLocal<External>(
      host::external_new(reinterpret_cast<uintptr_t>(value)));
}

void* External::Value(ExternalPointerTypeTag tag) const {
  return reinterpret_cast<void*>(host::external_value(Self(this)));
}

// --- Dates -----------------------------------------------------------------------

MaybeLocal<Value> Date::New(Local<Context> context, double time) {
  return Utils::ToMaybeLocal<Value>(host::date_new(time));
}

}  // namespace v8
