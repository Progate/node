// Initialization, the platform, tracing, cppgc and heap statistics.
//
// The host engine owns the heap and the garbage collector. cppgc objects are
// allocated and never collected; tracing is always disabled.

#include <stdlib.h>
#include <string.h>

#include <atomic>
#include <memory>
#include <new>

#include "src/shim.h"
#include "src/state.h"
#include "v8-version-string.h"

namespace cppgc {

class AllocationHandle {};

namespace internal {
class HeapBase {};
class FatalOutOfMemoryHandler {};
}  // namespace internal

}  // namespace cppgc

namespace v8 {

using shim::Address;

// --- V8 -------------------------------------------------------------------------

const char* V8::GetVersion() { return V8_VERSION_STRING "-browser-v8"; }

bool V8::Initialize(const int build_config) { return true; }
void V8::InitializePlatform(Platform* platform) {
  shim::GetState().platform = platform;
}
bool V8::Dispose() { return true; }
void V8::DisposePlatform() {}

// Flags tune V8's compilers and heap; the host engine has its own.
void V8::SetFlagsFromString(const char* str) {}
void V8::SetFlagsFromString(const char* str, size_t length) {}
// Every flag is taken (and removed, as V8 does with the flags it knows), so
// that the embedder does not report them as unknown.
void V8::SetFlagsFromCommandLine(int* argc, char** argv, bool remove_flags) {
  if (!remove_flags) return;
  int kept = 1;
  for (int i = 1; i < *argc; i++) {
    if (argv[i] != nullptr && argv[i][0] == '-') continue;
    argv[kept++] = argv[i];
  }
  *argc = kept;
}

void ResourceConstraints::ConfigureDefaults(uint64_t physical_memory,
                                            uint64_t virtual_memory_limit) {}

bool StartupData::CanBeRehashed() const { return false; }

HeapStatistics::HeapStatistics() { memset(static_cast<void*>(this), 0, sizeof(*this)); }
HeapSpaceStatistics::HeapSpaceStatistics() {
  memset(static_cast<void*>(this), 0, sizeof(*this));
}
HeapCodeStatistics::HeapCodeStatistics() {
  memset(static_cast<void*>(this), 0, sizeof(*this));
}

namespace internal {
class IsolateGroup {};
}  // namespace internal

namespace {
internal::IsolateGroup* DefaultGroup() {
  static auto* group = new internal::IsolateGroup();
  return group;
}
}  // namespace

IsolateGroup IsolateGroup::GetDefault() {
  internal::IsolateGroup* group = DefaultGroup();
  return IsolateGroup(std::move(group));
}

bool IsolateGroup::CanCreateNewGroups() { return false; }

IsolateGroup IsolateGroup::Create() { return GetDefault(); }

IsolateGroup::IsolateGroup(internal::IsolateGroup*&& isolate_group)
    : isolate_group_(isolate_group) {}
IsolateGroup::IsolateGroup(IsolateGroup&& other)
    : isolate_group_(other.isolate_group_) {}
IsolateGroup::IsolateGroup(const IsolateGroup& other)
    : isolate_group_(other.isolate_group_) {}
IsolateGroup& IsolateGroup::operator=(IsolateGroup&& other) {
  isolate_group_ = other.isolate_group_;
  return *this;
}
IsolateGroup& IsolateGroup::operator=(const IsolateGroup& other) {
  isolate_group_ = other.isolate_group_;
  return *this;
}
IsolateGroup::~IsolateGroup() = default;

// External memory is the host engine's business.
void ExternalMemoryAccounter::Increase(Isolate* isolate, size_t size) {}
void ExternalMemoryAccounter::Decrease(Isolate* isolate, size_t size) {}
void ExternalMemoryAccounter::Update(Isolate* isolate, int64_t delta) {}
ExternalMemoryAccounter::~ExternalMemoryAccounter() = default;

namespace internal {

void Internals::CheckInitializedImpl(v8::Isolate* isolate) {}

void VerifyHandleIsNonEmpty(bool is_empty) {
  if (is_empty) shim::Fatal("an empty handle was used");
}

// Traced references are globals the cppgc heap would trace.
Address* GlobalizeTracedReference(Isolate* isolate, Address value,
                                  Address* slot,
                                  TracedReferenceStoreMode store_mode,
                                  TracedReferenceHandling reference_handling) {
  return api_internal::GlobalizeReference(isolate, value);
}

// Moving hands the node over; what the target held is released.
void MoveTracedReference(Address** from, Address** to) {
  if (*from == *to) return;
  if (*to != nullptr) api_internal::DisposeGlobal(*to);
  *to = *from;
  *from = nullptr;
}

void CopyTracedReference(const Address* const* from, Address** to) {
  *to = api_internal::GlobalizeReference(nullptr, **from);
}

void DisposeTracedReference(Address* global_handle) {
  api_internal::DisposeGlobal(global_handle);
}

}  // namespace internal

// --- cppgc ------------------------------------------------------------------------

namespace internal {

class CppHeap final : public v8::CppHeap {
 public:
  CppHeap() = default;
};

}  // namespace internal

std::unique_ptr<CppHeap> CppHeap::Create(Platform* platform,
                                         const CppHeapCreateParams& params) {
  return std::make_unique<internal::CppHeap>();
}

cppgc::AllocationHandle& CppHeap::GetAllocationHandle() {
  static cppgc::AllocationHandle handle;
  return handle;
}

cppgc::HeapStatistics CppHeap::CollectStatistics(
    cppgc::HeapStatistics::DetailLevel detail_level) {
  return cppgc::HeapStatistics();
}

}  // namespace v8

namespace cppgc {

void InitializeProcess(PageAllocator* page_allocator,
                       size_t desired_heap_size) {}
void ShutdownProcess() {}

bool LivenessBroker::IsHeapObjectAliveImpl(const void* payload) const {
  return true;
}

void Visitor::CheckObjectNotInConstruction(const void* address) {}

namespace internal {

void* MakeGarbageCollectedTraitInternal::Allocate(AllocationHandle& handle,
                                                  size_t size,
                                                  GCInfoIndex index,
                                                  MixinTag) {
  void* memory = calloc(1, size);
  if (memory == nullptr) v8::shim::Fatal("cppgc: out of memory (%zu)", size);
  return memory;
}

std::atomic<GCInfoIndex> next_gc_info_index{1};

GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndex(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace,
    FinalizationCallback finalize, NameCallback name) {
  GCInfoIndex index = registered_index.load(std::memory_order_acquire);
  if (index == 0) {
    index = next_gc_info_index.fetch_add(1);
    registered_index.store(index, std::memory_order_release);
  }
  return index;
}

TraceDescriptor TraceTraitFromInnerAddressImpl::GetTraceDescriptor(
    const void* address) {
  return {address, nullptr};
}

void Fatal(const std::string& reason, const SourceLocation& location) {
  v8::shim::Fatal("cppgc: %s", reason.c_str());
}

void FatalImpl(const char* message, const SourceLocation& location) {
  v8::shim::Fatal("cppgc: %s", message);
}

namespace {

PersistentRegion& Region() {
  static HeapBase heap;
  static FatalOutOfMemoryHandler oom;
  static auto* region = new PersistentRegion(heap, oom);
  return *region;
}

}  // namespace

PersistentRegion& StrongPersistentPolicy::GetPersistentRegion(
    const void* object) {
  return Region();
}

PersistentRegion& WeakPersistentPolicy::GetPersistentRegion(
    const void* object) {
  return Region();
}

PersistentRegionBase::PersistentRegionBase(
    const FatalOutOfMemoryHandler& oom_handler)
    : oom_handler_(oom_handler) {}

PersistentRegionBase::~PersistentRegionBase() = default;

bool PersistentRegion::IsCreationThread() { return true; }

PersistentNode* PersistentRegionBase::RefillFreeListAndAllocateNode(
    void* owner, TraceRootCallback trace) {
  auto slots = std::make_unique<PersistentNodeSlots>();
  for (auto& node : *slots) {
    node.InitializeAsFreeNode(free_list_head_);
    free_list_head_ = &node;
  }
  nodes_.push_back(std::move(slots));
  return TryAllocateNodeFromFreeList(owner, trace);
}

}  // namespace internal
}  // namespace cppgc

// --- The platform: tracing is never on ----------------------------------------------

namespace v8::platform::tracing {

void TracingController::Initialize(TraceBuffer* trace_buffer) {
  trace_buffer_.reset(trace_buffer);
}

const uint8_t* TracingController::GetCategoryGroupEnabled(
    const char* category_group) {
  static const uint8_t disabled = 0;
  return &disabled;
}

uint64_t TracingController::AddTraceEvent(
    char phase, const uint8_t* category_enabled_flag, const char* name,
    const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags) {
  return 0;
}

uint64_t TracingController::AddTraceEventWithTimestamp(
    char phase, const uint8_t* category_enabled_flag, const char* name,
    const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags, int64_t timestamp) {
  return 0;
}

void TracingController::UpdateTraceEventDuration(
    const uint8_t* category_enabled_flag, const char* name, uint64_t handle) {}

void TracingController::StartTracing(TraceConfig* trace_config) {
  trace_config_.reset(trace_config);
}

void TracingController::StopTracing() {}

void TracingController::AddTraceStateObserver(
    v8::TracingController::TraceStateObserver* observer) {}

void TracingController::RemoveTraceStateObserver(
    v8::TracingController::TraceStateObserver* observer) {}

int64_t TracingController::CurrentTimestampMicroseconds() { return 0; }

int64_t TracingController::CurrentCpuTimestampMicroseconds() { return 0; }

void TraceConfig::AddIncludedCategory(const char* included_category) {
  included_categories_.push_back(included_category);
}

TraceBufferChunk::TraceBufferChunk(uint32_t seq) : seq_(seq) {}

void TraceBufferChunk::Reset(uint32_t new_seq) {
  next_free_ = 0;
  seq_ = new_seq;
}

TraceObject* TraceBufferChunk::AddTraceEvent(size_t* event_index) {
  *event_index = next_free_++;
  return &chunk_[*event_index];
}

TraceObject::~TraceObject() = default;

void TraceObject::Initialize(
    char phase, const uint8_t* category_enabled_flag, const char* name,
    const char* scope, uint64_t id, uint64_t bind_id, int num_args,
    const char** arg_names, const uint8_t* arg_types,
    const uint64_t* arg_values,
    std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
    unsigned int flags, int64_t timestamp, int64_t cpu_timestamp) {}

TraceWriter* TraceWriter::CreateJSONTraceWriter(std::ostream& stream) {
  return nullptr;
}

}  // namespace v8::platform::tracing
