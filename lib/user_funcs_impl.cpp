#include "gc_impl.h"

void* gc_malloc(size_t size) {
    void *ptr = malloc(size);
    GarbageCollector::GetInstance().AddAllocation(ptr, size);
    GarbageCollector::GetInstance().AddRoot(ptr);
    return ptr;
}

void* gc_malloc_manage(size_t size, FinalizerT finalizer) {
    void *ptr = malloc(size);
    GarbageCollector::GetInstance().AddAllocation(ptr, size, finalizer);
    GarbageCollector::GetInstance().AddRoot(ptr);
    return ptr;
}

void gc_delete_root(void *ptr) {
    GarbageCollector::GetInstance().DeleteRoot(ptr);
}

void gc_collect() {
    GarbageCollector::GetInstance().CollectGarbage();
}
