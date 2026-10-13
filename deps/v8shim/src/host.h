// What the host JavaScript engine provides to the shim: the `v8shim` imports.
// The host side is host/v8shim.mjs in @progate/browser-v8; this file is the
// contract between the two.
//
// Values are V8 addresses (see shim.h): a host table index tagged as a heap
// object, or a Smi. A function that can throw returns 0 (kNullAddress) or -1
// and leaves the exception pending on the host (exception_pending).

#ifndef V8SHIM_SRC_HOST_H_
#define V8SHIM_SRC_HOST_H_

#include <stddef.h>
#include <stdint.h>

#define V8SHIM_IMPORT(name) \
  __attribute__((import_module("v8shim"), import_name(#name)))

namespace v8::shim::host {

using Address = uintptr_t;

// What value_is asks. The host side (v8shim.ts) has the same numbers.
enum ValueKind : int32_t {
  kIsApiData,  // A template, context, script or module: Data but not a Value.
  kIsModule,
  kIsPrivate,
  kIsString,
  kIsArgumentsObject,
  kIsArray,
  kIsArrayBuffer,
  kIsArrayBufferView,
  kIsAsyncFunction,
  kIsBigInt,
  kIsBigInt64Array,
  kIsBigIntObject,
  kIsBigUint64Array,
  kIsBoolean,
  kIsBooleanObject,
  kIsDataView,
  kIsDate,
  kIsExternal,
  kIsFloat16Array,
  kIsFloat32Array,
  kIsFloat64Array,
  kIsFunction,
  kIsGeneratorFunction,
  kIsGeneratorObject,
  kIsInt16Array,
  kIsInt32,
  kIsInt32Array,
  kIsInt8Array,
  kIsMap,
  kIsMapIterator,
  kIsModuleNamespaceObject,
  kIsNativeError,
  kIsNumber,
  kIsNumberObject,
  kIsObject,
  kIsPromise,
  kIsProxy,
  kIsRegExp,
  kIsSet,
  kIsSetIterator,
  kIsSharedArrayBuffer,
  kIsStringObject,
  kIsSymbol,
  kIsSymbolObject,
  kIsTypedArray,
  kIsUint16Array,
  kIsUint32,
  kIsUint32Array,
  kIsUint8Array,
  kIsUint8ClampedArray,
  kIsWasmMemoryObject,
  kIsWeakMap,
  kIsWeakSet,
};

// What value_to converts to.
enum Conversion : int32_t {
  kToString,
  kToDetailString,
  kToObject,
  kTypeOf,
};

enum Equality : int32_t {
  kLooseEquals,
  kStrictEquals,
  kSameValue,
};

// How characters are laid out in wasm memory.
enum Encoding : int32_t {
  kUtf8,
  kLatin1,
  kUtf16,
};

enum WellKnownSymbol : int32_t {
  kSymbolIterator,
  kSymbolAsyncIterator,
  kSymbolToStringTag,
  kSymbolHasInstance,
  kSymbolToPrimitive,
  kSymbolUnscopables,
  kSymbolMatch,
  kSymbolReplace,
  kSymbolSearch,
  kSymbolSplit,
  kSymbolIsConcatSpreadable,
  kSymbolDispose,
  kSymbolAsyncDispose,
};

extern "C" {

// --- Table -----------------------------------------------------------------
// Scoped entries form a stack: scope_exit(mark) drops everything above mark.
V8SHIM_IMPORT(scope_enter) uint32_t scope_enter();
V8SHIM_IMPORT(scope_exit) void scope_exit(uint32_t mark);
// A new scoped entry holding `value` (keeps it alive in the current scope).
V8SHIM_IMPORT(scope_push) Address scope_push(Address value);
// Makes the entry `entry` (from scope_push) hold `value` instead.
V8SHIM_IMPORT(table_set) void table_set(Address entry, Address value);
// Global entries stay until disposed.
V8SHIM_IMPORT(global_new) Address global_new(Address value);
V8SHIM_IMPORT(global_dispose) void global_dispose(Address global);
// Weak globals call v8shim_weak_callback(node) once the value is collected.
V8SHIM_IMPORT(global_make_weak) void global_make_weak(Address global, Address* node);
V8SHIM_IMPORT(global_clear_weak) void global_clear_weak(Address global);
// Roots: undefined, the hole, null, true, false and "" (Internals root order).
V8SHIM_IMPORT(root) Address root(int32_t index);

// --- Identity and type -----------------------------------------------------
V8SHIM_IMPORT(same_value) int32_t same_value(Address a, Address b);
// V8's instance type for the inline checks of v8-internal.h: strings are
// below kFirstNonstringType, oddballs are kOddballType, objects kJSObjectType.
V8SHIM_IMPORT(instance_type) int32_t instance_type(Address value);
V8SHIM_IMPORT(oddball_kind) int32_t oddball_kind(Address value);
// A stable address per kind of value, standing in for a map.
V8SHIM_IMPORT(load_map) Address load_map(Address value);

// --- Values -------------------------------------------------------------------
V8SHIM_IMPORT(value_is) int32_t value_is(Address value, ValueKind kind);
// A conversion that can throw: 0 if it did.
V8SHIM_IMPORT(value_to) Address value_to(Address value, Conversion conversion);
V8SHIM_IMPORT(value_truthy) int32_t value_truthy(Address value);
// ToNumber into *out; 0 if it threw.
V8SHIM_IMPORT(value_to_number) int32_t value_to_number(Address value, double* out);
// 1 or 0; -1 if a loose comparison threw.
V8SHIM_IMPORT(value_equals) int32_t value_equals(Address a, Address b, Equality kind);
V8SHIM_IMPORT(number_new) Address number_new(double value);
V8SHIM_IMPORT(bigint_new) Address bigint_new(int64_t value, int32_t is_unsigned);
// The low 64 bits, signed or not; *lossless says whether that is the whole value.
V8SHIM_IMPORT(bigint_value) int64_t bigint_value(Address value, int32_t is_unsigned, int32_t* lossless);
V8SHIM_IMPORT(identity_hash) int32_t identity_hash(Address value);
// new Date(time).
V8SHIM_IMPORT(date_new) Address date_new(double time);
// The host engine's heap, in bytes: 0 used, 1 total, 2 limit.
V8SHIM_IMPORT(heap_size) double heap_size(int32_t which);

// --- Strings ------------------------------------------------------------------
V8SHIM_IMPORT(string_new) Address string_new(Encoding encoding, const void* data, uint32_t length);
// In UTF-16 code units.
V8SHIM_IMPORT(string_length) uint32_t string_length(Address string);
V8SHIM_IMPORT(string_is_one_byte) int32_t string_is_one_byte(Address string);
V8SHIM_IMPORT(string_utf8_length) uint32_t string_utf8_length(Address string);
// Copies `length` code units from `offset` as Latin-1 bytes or UTF-16 units.
V8SHIM_IMPORT(string_write) void string_write(Address string, Encoding encoding, uint32_t offset, uint32_t length, void* out);
// Writes whole UTF-8 sequences that fit; returns bytes, *characters the code units read.
V8SHIM_IMPORT(string_write_utf8) uint32_t string_write_utf8(Address string, char* out, uint32_t capacity, uint32_t* characters);
V8SHIM_IMPORT(string_concat) Address string_concat(Address left, Address right);

// --- Symbols, privates, externals -------------------------------------------
V8SHIM_IMPORT(symbol_new) Address symbol_new(Address description);
V8SHIM_IMPORT(symbol_description) Address symbol_description(Address symbol);
V8SHIM_IMPORT(symbol_well_known) Address symbol_well_known(WellKnownSymbol which);
// Symbol.for(description): the symbol registry shared by every context.
V8SHIM_IMPORT(symbol_for) Address symbol_for(Address description);
// for_api: the same name gives the same private.
V8SHIM_IMPORT(private_new) Address private_new(Address name, int32_t for_api);
V8SHIM_IMPORT(external_new) Address external_new(uintptr_t pointer);
V8SHIM_IMPORT(external_value) uintptr_t external_value(Address external);

// --- Objects ------------------------------------------------------------------
// Results of operations that can throw: -1 threw, otherwise 0 / 1.
enum DescriptorFlags : int32_t {
  kHasValue = 1 << 0,
  kHasGet = 1 << 1,
  kHasSet = 1 << 2,
  kHasWritable = 1 << 3,
  kWritable = 1 << 4,
  kHasEnumerable = 1 << 5,
  kEnumerable = 1 << 6,
  kHasConfigurable = 1 << 7,
  kConfigurable = 1 << 8,
};

enum CollectionKind : int32_t { kMap, kSet };

// The prototype, or 0 for Object.prototype.
V8SHIM_IMPORT(object_new) Address object_new(Address prototype);
V8SHIM_IMPORT(object_get) Address object_get(Address object, Address key);
// The context whose global `object` is (a vm context), or the null address.
V8SHIM_IMPORT(object_creation_context) Address object_creation_context(Address object);
// Own property, without interceptors (Object::HasRealNamedProperty).
V8SHIM_IMPORT(object_has_real) int32_t object_has_real(Address object, Address key);
// Without interceptors, along the prototype chain; the null address when absent.
V8SHIM_IMPORT(object_get_real) Address object_get_real(Address object, Address key);
V8SHIM_IMPORT(object_set) int32_t object_set(Address object, Address key, Address value);
V8SHIM_IMPORT(object_has) int32_t object_has(Address object, Address key);
V8SHIM_IMPORT(object_has_own) int32_t object_has_own(Address object, Address key);
V8SHIM_IMPORT(object_delete) int32_t object_delete(Address object, Address key);
V8SHIM_IMPORT(object_create_data_property) int32_t object_create_data_property(Address object, Address key, Address value);
V8SHIM_IMPORT(object_define_own) int32_t object_define_own(Address object, Address key, Address value, int32_t attributes);
// value/get/set are global entries or 0; flags are DescriptorFlags.
V8SHIM_IMPORT(object_define_descriptor) int32_t object_define_descriptor(Address object, Address key, Address value, Address get, Address set, int32_t flags);
// The descriptor object, or undefined.
V8SHIM_IMPORT(object_own_descriptor) Address object_own_descriptor(Address object, Address key);
// PropertyAttribute bits, -1 threw, -2 absent (reported as None).
V8SHIM_IMPORT(object_property_attributes) int32_t object_property_attributes(Address object, Address key, int32_t own);
V8SHIM_IMPORT(object_keys) Address object_keys(Address object, int32_t mode, int32_t filter, int32_t index_filter, int32_t conversion);
V8SHIM_IMPORT(object_get_prototype) Address object_get_prototype(Address object);
V8SHIM_IMPORT(object_set_prototype) int32_t object_set_prototype(Address object, Address prototype);
V8SHIM_IMPORT(object_constructor_name) Address object_constructor_name(Address object);
V8SHIM_IMPORT(object_set_integrity) int32_t object_set_integrity(Address object, int32_t level);
V8SHIM_IMPORT(object_is_constructor) int32_t object_is_constructor(Address object);
V8SHIM_IMPORT(object_preview_entries) Address object_preview_entries(Address object, int32_t* is_key_value);
V8SHIM_IMPORT(private_set) void private_set(Address object, Address key, Address value);
V8SHIM_IMPORT(private_get) Address private_get(Address object, Address key);
V8SHIM_IMPORT(private_has) int32_t private_has(Address object, Address key);
V8SHIM_IMPORT(private_delete) void private_delete(Address object, Address key);
V8SHIM_IMPORT(internal_field_count) int32_t internal_field_count(Address object);
V8SHIM_IMPORT(internal_field_set) void internal_field_set(Address object, int32_t index, Address value);
V8SHIM_IMPORT(internal_field_get) Address internal_field_get(Address object, int32_t index);
V8SHIM_IMPORT(internal_pointer_set) void internal_pointer_set(Address object, int32_t index, uintptr_t pointer);
V8SHIM_IMPORT(internal_pointer_get) uintptr_t internal_pointer_get(Address object, int32_t index);
V8SHIM_IMPORT(object_wrap) void object_wrap(Address object, uintptr_t pointer);
V8SHIM_IMPORT(object_unwrap) uintptr_t object_unwrap(Address object);
V8SHIM_IMPORT(object_set_native_property) void object_set_native_property(Address object, Address name, uintptr_t getter, uintptr_t setter, Address data, int32_t attributes, int32_t lazy);

// --- Functions, arrays, collections, promises -------------------------------
V8SHIM_IMPORT(function_call) Address function_call(Address function, Address receiver, int32_t argc, const Address* argv);
V8SHIM_IMPORT(function_construct) Address function_construct(Address function, int32_t argc, const Address* argv);
V8SHIM_IMPORT(function_set_name) void function_set_name(Address function, Address name);
V8SHIM_IMPORT(function_script_name) Address function_script_name(Address function);
V8SHIM_IMPORT(array_new) Address array_new(uint32_t length);
V8SHIM_IMPORT(array_from) Address array_from(const Address* elements, uint32_t length);
V8SHIM_IMPORT(array_length) uint32_t array_length(Address array);
V8SHIM_IMPORT(collection_new) Address collection_new(CollectionKind kind);
// Map: set(key, value); Set: add(key). -1 if it threw.
V8SHIM_IMPORT(collection_add) int32_t collection_add(Address collection, Address key, Address value);
// Map.prototype.get; the null address when it threw.
V8SHIM_IMPORT(map_get) Address map_get(Address map, Address key);
// Map/Set.prototype.delete: 1 when it was there, 0 when not, -1 when it threw.
V8SHIM_IMPORT(collection_delete) int32_t collection_delete(Address collection, Address key);
V8SHIM_IMPORT(resolver_new) Address resolver_new();
V8SHIM_IMPORT(resolver_promise) Address resolver_promise(Address resolver);
V8SHIM_IMPORT(resolver_settle) void resolver_settle(Address resolver, Address value, int32_t reject);
V8SHIM_IMPORT(promise_then) Address promise_then(Address promise, Address on_fulfilled, Address on_rejected);
// Promise::PromiseState: 0 pending, 1 fulfilled, 2 rejected.
V8SHIM_IMPORT(promise_state) int32_t promise_state(Address promise);
V8SHIM_IMPORT(promise_result) Address promise_result(Address promise);
V8SHIM_IMPORT(promise_mark_handled) void promise_mark_handled(Address promise);
// 0 the target, 1 the handler.
V8SHIM_IMPORT(proxy_part) Address proxy_part(Address proxy, int32_t which);
V8SHIM_IMPORT(regexp_new) Address regexp_new(Address pattern, int32_t flags);
V8SHIM_IMPORT(regexp_exec) Address regexp_exec(Address regexp, Address subject);
V8SHIM_IMPORT(json_stringify) Address json_stringify(Address value, Address gap);

// --- Templates and calls into C++ ------------------------------------------------
enum TemplatePart : int32_t { kInstanceTemplate, kPrototypeTemplate };

V8SHIM_IMPORT(template_new_function) Address template_new_function(uintptr_t callback, Address data, int32_t length, int32_t constructable);
// 0 or a FunctionTemplate.
V8SHIM_IMPORT(template_new_object) Address template_new_object(Address constructor);
V8SHIM_IMPORT(template_function) Address template_function(Address function_template);
V8SHIM_IMPORT(template_new_instance) Address template_new_instance(Address object_template);
V8SHIM_IMPORT(template_part) Address template_part(Address function_template, TemplatePart part);
V8SHIM_IMPORT(template_inherit) void template_inherit(Address function_template, Address parent);
V8SHIM_IMPORT(template_set_class_name) void template_set_class_name(Address function_template, Address name);
V8SHIM_IMPORT(template_set_length) void template_set_length(Address function_template, int32_t length);
V8SHIM_IMPORT(template_read_only_prototype) void template_read_only_prototype(Address function_template);
V8SHIM_IMPORT(template_has_instance) int32_t template_has_instance(Address function_template, Address value);
V8SHIM_IMPORT(template_set_internal_field_count) void template_set_internal_field_count(Address object_template, int32_t count);
// callbacks: getter, setter, query, deleter, enumerator, descriptor (0 if none).
V8SHIM_IMPORT(template_set_handler) void template_set_handler(Address object_template, int32_t indexed, const uintptr_t* callbacks, Address data, int32_t flags);
V8SHIM_IMPORT(template_set) void template_set(Address tmpl, Address name, Address value, int32_t attributes);
V8SHIM_IMPORT(template_set_accessor_property) void template_set_accessor_property(Address tmpl, Address name, Address getter, Address setter, int32_t attributes);
V8SHIM_IMPORT(template_set_native_property) void template_set_native_property(Address tmpl, Address name, uintptr_t getter, uintptr_t setter, Address data, int32_t attributes, int32_t lazy);
// The data of a FunctionTemplate, or of an accessor / interceptor record.
V8SHIM_IMPORT(template_data) Address template_data(Address target);
// A call frame laid out (v8shim_call_function / v8shim_call_property): the
// host writes the receiver, the arguments and new.target into the slots.
V8SHIM_IMPORT(frame_enter) void frame_enter(uint32_t frame, Address* return_slot, Address* receiver_slot, Address* argument_slots, Address* new_target_slot);
V8SHIM_IMPORT(frame_leave) void frame_leave(uint32_t frame, Address return_value);

// --- Contexts and scripts ------------------------------------------------------
V8SHIM_IMPORT(context_new) Address context_new(Address global_template, Address global_object);
// The context's global entry, whatever handle it came through.
V8SHIM_IMPORT(context_id) Address context_id(Address context);
V8SHIM_IMPORT(context_global) Address context_global(Address context);
V8SHIM_IMPORT(context_extras) Address context_extras(Address context);
// Isolate::{Get,Set}ContinuationPreservedEmbedderDataV2. The host keeps it, as
// the extras binding's functions of the same name read and write it too.
V8SHIM_IMPORT(continuation_data) Address continuation_data();
V8SHIM_IMPORT(set_continuation_data) void set_continuation_data(Address data);
// `host_defined_options` (0 for none) is what a dynamic import() in the code
// passes back to v8shim_import_dynamic.
V8SHIM_IMPORT(script_compile) Address script_compile(Address source, Address name, int32_t line_offset, int32_t column_offset, Address host_defined_options);
// Runs in `context` (the global of a vm context is its own; see the host).
V8SHIM_IMPORT(script_run) Address script_run(Address script, Address context);
// 0 the sourceURL, 1 the sourceMappingURL.
V8SHIM_IMPORT(script_url) Address script_url(Address script, int32_t which);
V8SHIM_IMPORT(compile_function) Address compile_function(Address source, Address name, int32_t line_offset, int32_t column_offset, const Address* parameters, uint32_t parameter_count, const Address* extensions, uint32_t extension_count, Address host_defined_options, Address context);

// --- Modules -------------------------------------------------------------------
// A module is a host record: a source text module (compiled by the host into
// a function it runs at evaluation) or a synthetic module.
enum ModuleFlag : int32_t { kModuleIsSourceText, kModuleIsGraphAsync, kModuleHasTopLevelAwait };
enum ModuleRequestPart : int32_t { kRequestSpecifier, kRequestAttributes };
enum ModuleRequestNumber : int32_t { kRequestPhase, kRequestSourceOffset };
V8SHIM_IMPORT(module_compile) Address module_compile(Address source, Address name, int32_t line_offset, int32_t column_offset, Address host_defined_options);
// An array of ModuleRequest records, in source order.
V8SHIM_IMPORT(module_requests) Address module_requests(Address module);
V8SHIM_IMPORT(module_request_part) Address module_request_part(Address request, ModuleRequestPart part);
V8SHIM_IMPORT(module_request_number) int32_t module_request_number(Address request, ModuleRequestNumber which);
// Module::Status.
V8SHIM_IMPORT(module_status) int32_t module_status(Address module);
V8SHIM_IMPORT(module_exception) Address module_exception(Address module);
V8SHIM_IMPORT(module_namespace) Address module_namespace(Address module);
V8SHIM_IMPORT(module_flag) int32_t module_flag(Address module, ModuleFlag flag);
// 0 the sourceURL, 1 the sourceMappingURL of a source text module.
V8SHIM_IMPORT(module_url) Address module_url(Address module, int32_t which);
// Links the graph: for each request not linked yet, the host calls
// v8shim_resolve_module(callback, by_index, ...). 1, or 0 with the exception
// pending.
V8SHIM_IMPORT(module_instantiate) int32_t module_instantiate(Address context, Address module, uintptr_t callback, int32_t by_index);
// The promise of the evaluation, or 0 with the exception pending.
V8SHIM_IMPORT(module_evaluate) Address module_evaluate(Address context, Address module);
// A synthetic module; its evaluation calls v8shim_synthetic_steps(steps, ...).
V8SHIM_IMPORT(module_synthetic_new) Address module_synthetic_new(Address name, Address export_names, uintptr_t steps);
// 1, or -1 with the exception pending (not one of its export names).
V8SHIM_IMPORT(module_synthetic_set) int32_t module_synthetic_set(Address module, Address name, Address value);

// --- Errors, messages, stack traces ---------------------------------------------
enum ErrorKind : int32_t { kError, kRangeError, kReferenceError, kSyntaxError, kTypeError };
enum MessagePart : int32_t { kMessageText, kMessageResourceName, kMessageSourceLine, kMessageStackTrace };
enum MessageNumber : int32_t { kMessageLine, kMessageStartColumn, kMessageEndColumn };
enum FrameField : int32_t { kFrameLine, kFrameColumn, kFrameScriptId, kFrameIsEval, kFrameScriptName, kFrameFunctionName };

V8SHIM_IMPORT(error_new) Address error_new(ErrorKind kind, Address message, Address options);
V8SHIM_IMPORT(message_new) Address message_new(Address exception);
V8SHIM_IMPORT(message_part) Address message_part(Address message, MessagePart part);
V8SHIM_IMPORT(message_number) int32_t message_number(Address message, MessageNumber which);
V8SHIM_IMPORT(stack_trace_current) Address stack_trace_current(int32_t frame_limit);
V8SHIM_IMPORT(frame_number) int32_t frame_number(Address frame, FrameField field);
V8SHIM_IMPORT(frame_part) Address frame_part(Address frame, FrameField field);

// --- Microtasks ----------------------------------------------------------------
V8SHIM_IMPORT(microtask_enqueue) void microtask_enqueue(Address function);
// Runs v8shim_run_microtask(callback, data, value) as a microtask.
V8SHIM_IMPORT(microtask_enqueue_native) void microtask_enqueue_native(uintptr_t callback, uintptr_t data, Address value);
// Suspends the program (JSPI) until the host engine's microtask queue is empty.
// Only callable when microtask_can_suspend() says so: with JS frames between
// here and the entry point, the engine cannot suspend and throws on the call.
V8SHIM_IMPORT(microtask_checkpoint) void microtask_checkpoint();
V8SHIM_IMPORT(microtask_can_suspend) int32_t microtask_can_suspend();

// --- ArrayBuffers ----------------------------------------------------------------
// A JavaScript ArrayBuffer cannot point into wasm memory; a backing store
// there is its mirror. `holder` is a std::shared_ptr<BackingStore>* the host
// releases (v8shim_store_release) once the ArrayBuffer is collected.
enum ViewNumber : int32_t { kViewByteOffset, kViewByteLength, kViewLength };
enum ViewKind : int32_t {
  kUint8Array,
  kUint8ClampedArray,
  kInt8Array,
  kUint16Array,
  kInt16Array,
  kUint32Array,
  kInt32Array,
  kFloat16Array,
  kFloat32Array,
  kFloat64Array,
  kBigInt64Array,
  kBigUint64Array,
  kDataView,
};

// A new (Shared)ArrayBuffer holding a copy of `data`, with the store as its mirror.
// `aliased`: C++ made it empty to share it with JS, which both keep writing
// (node's AliasedBuffer), rather than to hand over contents it wrote.
V8SHIM_IMPORT(array_buffer_new) Address array_buffer_new(void* holder, uintptr_t data, uint32_t length, int32_t shared, int32_t aliased);
// The buffer's mirror (made if needed), brought up to date with the buffer.
V8SHIM_IMPORT(array_buffer_pin) uintptr_t array_buffer_pin(Address buffer);
// WebAssembly.Memory#buffer.
V8SHIM_IMPORT(wasm_memory_buffer) Address wasm_memory_buffer(Address memory);
// SharedArrayBuffers shared between isolates (node's worker_threads). Each
// isolate runs in its own host engine, so the host passes the object itself
// between them; a backing store from shared_buffer_token stands for it by its
// data address and carries no copy of the contents.
// The token's holder (std::shared_ptr<BackingStore>*) for the buffer.
V8SHIM_IMPORT(shared_buffer_token) uintptr_t shared_buffer_token(Address buffer);
// The same for any object the hosts pass between isolates by serial number
// (a WebAssembly.Module): the object's serial (passing it on to the other
// threads), whether this host has it, waiting for it, and the object (0 when
// it did not arrive).
V8SHIM_IMPORT(shared_object_serial) uint32_t shared_object_serial(Address value);
V8SHIM_IMPORT(shared_object_has) int32_t shared_object_has(uint32_t serial);
V8SHIM_IMPORT(shared_object_wait) int32_t shared_object_wait(uint32_t serial);
V8SHIM_IMPORT(shared_object_get) Address shared_object_get(uint32_t serial);
// Whether this isolate's host has the buffer the token stands for.
V8SHIM_IMPORT(shared_buffer_has) int32_t shared_buffer_has(uintptr_t data);
// Waits until it has: the buffer is on its way from the isolate that made the
// token. Suspends the thread (JSPI), so only when microtask_can_suspend().
V8SHIM_IMPORT(shared_buffer_wait) int32_t shared_buffer_wait(uintptr_t data);
V8SHIM_IMPORT(array_buffer_length) uint32_t array_buffer_length(Address buffer);
V8SHIM_IMPORT(array_buffer_was_detached) int32_t array_buffer_was_detached(Address buffer);
V8SHIM_IMPORT(array_buffer_detach) void array_buffer_detach(Address buffer);
V8SHIM_IMPORT(array_buffer_copy) uint32_t array_buffer_copy(Address source, uint32_t source_offset, Address destination, uint32_t destination_offset, uint32_t length);
V8SHIM_IMPORT(view_buffer) Address view_buffer(Address view);
V8SHIM_IMPORT(view_number) uint32_t view_number(Address view, ViewNumber which);
// Copies up to `length` bytes of the view into wasm memory; returns how many.
V8SHIM_IMPORT(view_copy) uint32_t view_copy(Address view, void* destination, uint32_t length);
V8SHIM_IMPORT(typed_array_new) Address typed_array_new(ViewKind kind, Address buffer, uint32_t byte_offset, uint32_t length);

// --- ValueSerializer / ValueDeserializer -----------------------------------------
// The host writes and reads V8's wire format. It passes `delegate` back to
// v8shim_serializer_* / v8shim_deserializer_* for host objects and
// SharedArrayBuffer ids.
V8SHIM_IMPORT(serializer_new) int32_t serializer_new(void* delegate);
V8SHIM_IMPORT(serializer_delete) void serializer_delete(int32_t id);
V8SHIM_IMPORT(serializer_write_header) void serializer_write_header(int32_t id);
// 1 when written, 0 with the exception pending, -1 when the value cannot be
// cloned (serializer_error has the message for the delegate).
V8SHIM_IMPORT(serializer_write_value) int32_t serializer_write_value(int32_t id, Address value);
V8SHIM_IMPORT(serializer_error) Address serializer_error(int32_t id);
V8SHIM_IMPORT(serializer_size) uint32_t serializer_size(int32_t id);
// Copies the bytes to `destination` (serializer_size of them) and starts over.
V8SHIM_IMPORT(serializer_release) void serializer_release(int32_t id, void* destination);
V8SHIM_IMPORT(serializer_transfer) void serializer_transfer(int32_t id, uint32_t transfer_id, Address buffer);
V8SHIM_IMPORT(serializer_treat_views) void serializer_treat_views(int32_t id, int32_t mode);
V8SHIM_IMPORT(serializer_write_uint32) void serializer_write_uint32(int32_t id, uint32_t value);
V8SHIM_IMPORT(serializer_write_uint64) void serializer_write_uint64(int32_t id, uint64_t value);
V8SHIM_IMPORT(serializer_write_double) void serializer_write_double(int32_t id, double value);
V8SHIM_IMPORT(serializer_write_raw) void serializer_write_raw(int32_t id, const void* source, uint32_t length);

// The host keeps a copy of the bytes; read_raw gives an offset into them.
V8SHIM_IMPORT(deserializer_new) int32_t deserializer_new(void* delegate, const void* data, uint32_t size);
V8SHIM_IMPORT(deserializer_delete) void deserializer_delete(int32_t id);
// 1, or 0 with the exception pending.
V8SHIM_IMPORT(deserializer_read_header) int32_t deserializer_read_header(int32_t id);
V8SHIM_IMPORT(deserializer_read_value) Address deserializer_read_value(int32_t id);
V8SHIM_IMPORT(deserializer_transfer) void deserializer_transfer(int32_t id, uint32_t transfer_id, Address buffer);
V8SHIM_IMPORT(deserializer_version) uint32_t deserializer_version(int32_t id);
// 1 when read, 0 at the end of the data.
V8SHIM_IMPORT(deserializer_read_uint32) int32_t deserializer_read_uint32(int32_t id, uint32_t* value);
V8SHIM_IMPORT(deserializer_read_uint64) int32_t deserializer_read_uint64(int32_t id, uint64_t* value);
V8SHIM_IMPORT(deserializer_read_double) int32_t deserializer_read_double(int32_t id, double* value);
// The offset of `length` bytes, or -1 past the end.
V8SHIM_IMPORT(deserializer_read_raw) int32_t deserializer_read_raw(int32_t id, uint32_t length);

// --- Exceptions ---------------------------------------------------------------
// One exception can be pending: a call that threw, or Isolate::ThrowException.
// A callback returning to JavaScript throws it.
V8SHIM_IMPORT(exception_throw) void exception_throw(Address exception);
V8SHIM_IMPORT(exception_pending) Address exception_pending();
V8SHIM_IMPORT(exception_clear) void exception_clear();

// --- Isolate callbacks ---------------------------------------------------------
// Whether the host should call v8shim_promise_reject / v8shim_prepare_stack_trace.
V8SHIM_IMPORT(set_promise_reject_callback) void set_promise_reject_callback(int32_t enabled);
V8SHIM_IMPORT(set_prepare_stack_trace) void set_prepare_stack_trace(int32_t enabled);

// --- Calls into C++ ----------------------------------------------------------
// A callback stored its return value into the frame slot at `slot`.
V8SHIM_IMPORT(set_return_value) void set_return_value(Address* slot, Address value);

}  // extern "C"

}  // namespace v8::shim::host

#endif  // V8SHIM_SRC_HOST_H_
