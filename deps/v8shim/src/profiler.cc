// Profilers. The host engine profiles itself (its devtools); the API reports
// that there is nothing to profile with.

#include "src/shim.h"

namespace v8 {

namespace {

// HeapProfiler is used through a pointer and holds no data.
alignas(void*) char heap_profiler[sizeof(void*)];

}  // namespace

HeapProfiler* Isolate::GetHeapProfiler() {
  return reinterpret_cast<HeapProfiler*>(heap_profiler);
}

void HeapProfiler::AddBuildEmbedderGraphCallback(
    BuildEmbedderGraphCallback callback, void* data) {}

void HeapProfiler::RemoveBuildEmbedderGraphCallback(
    BuildEmbedderGraphCallback callback, void* data) {}

const HeapSnapshot* HeapProfiler::TakeHeapSnapshot(
    const HeapSnapshotOptions& options) {
  return nullptr;
}

void HeapProfiler::StartTrackingHeapObjects(bool track_allocations) {}

bool HeapProfiler::StartSamplingHeapProfiler(uint64_t sample_interval,
                                             int stack_depth,
                                             SamplingFlags flags) {
  return false;
}

void HeapProfiler::StopSamplingHeapProfiler() {}

AllocationProfile* HeapProfiler::GetAllocationProfile() { return nullptr; }

void HeapProfiler::QueryObjects(Local<Context> context,
                                QueryObjectPredicate* predicate,
                                std::vector<Global<Object>>* objects) {}

void HeapSnapshot::Delete() {}

void HeapSnapshot::Serialize(OutputStream* stream,
                             SerializationFormat format) const {
  stream->EndOfStream();
}

CpuProfiler* CpuProfiler::New(Isolate* isolate, CpuProfilingNamingMode mode,
                              CpuProfilingLoggingMode logging_mode) {
  return nullptr;
}

void CpuProfiler::UseDetailedSourcePositionsForProfiling(Isolate* isolate) {}

void CpuProfiler::Dispose() {}

CpuProfilingResult CpuProfiler::Start(
    CpuProfilingOptions options,
    std::unique_ptr<DiscardedSamplesDelegate> delegate) {
  return {0, CpuProfilingStatus::kErrorTooManyProfilers};
}

CpuProfile* CpuProfiler::Stop(ProfilerId id) { return nullptr; }

void CpuProfile::Delete() {}

void CpuProfile::Serialize(OutputStream* stream,
                           SerializationFormat format) const {
  stream->EndOfStream();
}

}  // namespace v8
