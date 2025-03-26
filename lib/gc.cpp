#include "gc_impl.h"

void* gc_malloc(size_t size) {
    void *ptr = malloc(size);
    GarbageCollector::GetInstance().AddAllocation(ptr, size);
    return ptr;
}

void* gc_malloc_manage(size_t size, FinalizerT finalizer) {
    void *ptr = malloc(size);
    GarbageCollector::GetInstance().AddAllocation(ptr, size, finalizer);
    GarbageCollector::GetInstance().AddRoot(ptr);
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

void gc_collect() {
    GarbageCollector::GetInstance().CollectGarbage();
}
