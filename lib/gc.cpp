#include "gc_impl.h"

void* gc_malloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr) {
        GarbageCollector::GetInstance().AddAllocation(ptr, size);
    }
    return ptr;
}

void* gc_malloc_manage(size_t size, FinalizerT finalizer) {
    void *ptr = malloc(size);
    if (ptr) {
        GarbageCollector::GetInstance().AddAllocation(ptr, size, finalizer);
    }
    return ptr;
}

void gc_add_edge(void *parent, void *child) {
    GarbageCollector::GetInstance().AddEdge(parent, child);
}

void gc_del_edge(void *parent, void *child) {
    GarbageCollector::GetInstance().DeleteEdge(parent, child);
}

void gc_add_root(void *ptr) {
    GarbageCollector::GetInstance().AddRoot(ptr);
}

void gc_delete_root(void *ptr) {
    GarbageCollector::GetInstance().DeleteRoot(ptr);
}

void gc_block_collect() {
    GarbageCollector::GetInstance().BlockCollect();
}

void gc_unlock_collect() {
    GarbageCollector::GetInstance().UnlockCollect();
}

void gc_collect() {
    GarbageCollector::GetInstance().CollectGarbage();
}


void gc_start_incremental_mark() {
    GarbageCollector::GetInstance().StartIncrementalMark();
}

void gc_step_mark() {
    GarbageCollector::GetInstance().StepMark();
}

bool gc_is_marking() {
    return GarbageCollector::GetInstance().IsMarking();
}

void gc_finish_incremental_mark() {
    GarbageCollector::GetInstance().FinishIncrementalMark();
}

void gc_start_background_collector(size_t steps, int interval_ms) {
    GarbageCollector::GetInstance().StartBackgroundCollector(steps, interval_ms);
}

void gc_stop_background_collector() {
    GarbageCollector::GetInstance().StopBackgroundCollector();
}

bool gc_is_background_collector_running() {
    return GarbageCollector::GetInstance().IsBackgroundCollectorRunning();
}
