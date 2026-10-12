// ES modules. The host compiles a source text module into a function it runs
// at evaluation, links the graph by calling back into the resolve callback the
// embedder passes to InstantiateModule(), and keeps the namespace objects.
// Synthetic modules (node's builtins and CommonJS facades) are host records
// whose evaluation calls the embedder's evaluation steps.

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

}  // namespace

MaybeLocal<Module> ScriptCompiler::CompileModule(
    Isolate* isolate, Source* source, CompileOptions options,
    NoCacheReason no_cache_reason) {
  if (source->cached_data) source->cached_data->rejected = true;
  return Utils::ToMaybeLocal<Module>(host::module_compile(
      Utils::Open(source->source_string), Utils::Open(source->resource_name),
      source->resource_line_offset, source->resource_column_offset,
      Utils::Open(source->host_defined_options)));
}

Module::Status Module::GetStatus() const {
  return static_cast<Status>(host::module_status(Self(this)));
}

Local<Value> Module::GetException() const {
  return Utils::ToLocal<Value>(host::module_exception(Self(this)));
}

Local<FixedArray> Module::GetModuleRequests() const {
  return Utils::ToLocal<FixedArray>(host::module_requests(Self(this)));
}

int Module::GetIdentityHash() const { return host::identity_hash(Self(this)); }

Maybe<bool> Module::InstantiateModule(Local<Context> context,
                                      ResolveModuleCallback module_callback,
                                      ResolveSourceCallback source_callback) {
  if (!host::module_instantiate(Utils::Open(context), Self(this),
                                reinterpret_cast<uintptr_t>(module_callback),
                                0))
    return Nothing<bool>();
  return Just(true);
}

Maybe<bool> Module::InstantiateModule(
    Local<Context> context, ResolveModuleByIndexCallback module_callback,
    ResolveSourceByIndexCallback source_callback) {
  if (!host::module_instantiate(Utils::Open(context), Self(this),
                                reinterpret_cast<uintptr_t>(module_callback),
                                1))
    return Nothing<bool>();
  return Just(true);
}

MaybeLocal<Value> Module::Evaluate(Local<Context> context) {
  return Utils::ToMaybeLocal<Value>(
      host::module_evaluate(Utils::Open(context), Self(this)));
}

Local<Value> Module::GetModuleNamespace() {
  return Utils::ToLocal<Value>(host::module_namespace(Self(this)));
}

// The module stands for its own unbound script.
Local<UnboundModuleScript> Module::GetUnboundModuleScript() {
  return Utils::ToLocal<UnboundModuleScript>(Self(this));
}

Local<Value> UnboundModuleScript::GetSourceURL() {
  return Utils::ToLocal<Value>(host::module_url(Self(this), 0));
}

Local<Value> UnboundModuleScript::GetSourceMappingURL() {
  return Utils::ToLocal<Value>(host::module_url(Self(this), 1));
}

bool Module::IsSourceTextModule() const {
  return host::module_flag(Self(this), host::kModuleIsSourceText) != 0;
}

bool Module::IsGraphAsync() const {
  return host::module_flag(Self(this), host::kModuleIsGraphAsync) != 0;
}

bool Module::HasTopLevelAwait() const {
  return host::module_flag(Self(this), host::kModuleHasTopLevelAwait) != 0;
}

// The host engine does not say which awaits never settled.
std::pair<LocalVector<Module>, LocalVector<Message>>
Module::GetStalledTopLevelAwaitMessages(Isolate* isolate) {
  return {LocalVector<Module>(isolate), LocalVector<Message>(isolate)};
}

Local<Module> Module::CreateSyntheticModule(
    Isolate* isolate, Local<String> module_name,
    const MemorySpan<const Local<String>>& export_names,
    SyntheticModuleEvaluationSteps evaluation_steps) {
  std::vector<Address> names;
  names.reserve(export_names.size());
  for (Local<String> name : export_names) names.push_back(Utils::Open(name));
  Address array =
      host::array_from(names.data(), static_cast<uint32_t>(names.size()));
  return Utils::ToLocal<Module>(host::module_synthetic_new(
      Utils::Open(module_name), array,
      reinterpret_cast<uintptr_t>(evaluation_steps)));
}

Maybe<bool> Module::SetSyntheticModuleExport(Isolate* isolate,
                                             Local<String> export_name,
                                             Local<Value> export_value) {
  if (host::module_synthetic_set(Self(this), Utils::Open(export_name),
                                 Utils::Open(export_value)) < 0)
    return Nothing<bool>();
  return Just(true);
}

Local<String> ModuleRequest::GetSpecifier() const {
  return Utils::ToLocal<String>(
      host::module_request_part(Self(this), host::kRequestSpecifier));
}

Local<FixedArray> ModuleRequest::GetImportAttributes() const {
  return Utils::ToLocal<FixedArray>(
      host::module_request_part(Self(this), host::kRequestAttributes));
}

ModuleImportPhase ModuleRequest::GetPhase() const {
  return static_cast<ModuleImportPhase>(
      host::module_request_number(Self(this), host::kRequestPhase));
}

int ModuleRequest::GetSourceOffset() const {
  return host::module_request_number(Self(this), host::kRequestSourceOffset);
}

// --- Calls from the host ----------------------------------------------------------
// Each opens a HandleScope (the embedder's callbacks escape their results into
// it) and hands its result back as a global entry, which outlives the scope;
// the host reads it and disposes of it. 0 when the callback threw.

namespace {

Address Keep(Address value) {
  return value == 0 ? 0 : host::global_new(value);
}

}  // namespace

// Resolves one request of `referrer` with the embedder's callback.
extern "C" __attribute__((export_name("v8shim_resolve_module"))) Address
v8shim_resolve_module(uintptr_t callback, int32_t by_index, Address context,
                      Address referrer, int32_t index, Address specifier,
                      Address attributes) {
  HandleScope scope(shim::TheIsolate());
  MaybeLocal<Module> resolved;
  if (by_index) {
    auto resolve =
        reinterpret_cast<Module::ResolveModuleByIndexCallback>(callback);
    resolved = resolve(Utils::ToLocal<Context>(context),
                       static_cast<size_t>(index),
                       Utils::ToLocal<Module>(referrer));
  } else {
    auto resolve = reinterpret_cast<Module::ResolveModuleCallback>(callback);
    resolved = resolve(Utils::ToLocal<Context>(context),
                       Utils::ToLocal<String>(specifier),
                       Utils::ToLocal<FixedArray>(attributes),
                       Utils::ToLocal<Module>(referrer));
  }
  return Keep(Utils::Open(resolved));
}

// Runs a synthetic module's evaluation steps.
extern "C" __attribute__((export_name("v8shim_synthetic_steps"))) Address
v8shim_synthetic_steps(uintptr_t steps, Address context, Address module) {
  HandleScope scope(shim::TheIsolate());
  auto evaluate =
      reinterpret_cast<Module::SyntheticModuleEvaluationSteps>(steps);
  return Keep(Utils::Open(evaluate(Utils::ToLocal<Context>(context),
                                   Utils::ToLocal<Module>(module))));
}

// A module read import.meta for the first time.
extern "C" __attribute__((export_name("v8shim_import_meta"))) void
v8shim_import_meta(Address context, Address module, Address meta) {
  HostInitializeImportMetaObjectCallback initialize =
      shim::GetState().initialize_import_meta;
  if (initialize == nullptr) return;
  HandleScope scope(shim::TheIsolate());
  initialize(Utils::ToLocal<Context>(context), Utils::ToLocal<Module>(module),
             Utils::ToLocal<Object>(meta));
}

// import() in a script, function or module. The promise, or 0 with the
// exception pending.
extern "C" __attribute__((export_name("v8shim_import_dynamic"))) Address
v8shim_import_dynamic(Address context, Address host_defined_options,
                      Address resource_name, Address specifier, int32_t phase,
                      Address attributes) {
  shim::State& state = shim::GetState();
  HandleScope scope(shim::TheIsolate());
  Local<Context> ctx = Utils::ToLocal<Context>(context);
  Local<Data> options = Utils::ToLocal<Data>(host_defined_options);
  Local<Value> name = Utils::ToLocal<Value>(resource_name);
  Local<String> spec = Utils::ToLocal<String>(specifier);
  Local<FixedArray> attrs = Utils::ToLocal<FixedArray>(attributes);
  if (state.import_module_with_phase_dynamically != nullptr)
    return Keep(Utils::Open(state.import_module_with_phase_dynamically(
        ctx, options, name, spec, static_cast<ModuleImportPhase>(phase),
        attrs)));
  if (state.import_module_dynamically != nullptr)
    return Keep(Utils::Open(
        state.import_module_dynamically(ctx, options, name, spec, attrs)));
  return 0;
}

}  // namespace v8
