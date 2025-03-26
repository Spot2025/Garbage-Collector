#ifndef GC_IMPL_H
#define GC_IMPL_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

#include "gc.h"

inline void DefaultFinalizer(void *ptr, size_t size) {
    return;
}

class GarbageCollector {
    struct Allocation {
        size_t size_;
        void *ptr_;
        bool marked_;
        std::unordered_set<void*> edges;
        FinalizerT finalizer_;
    };

    std::unordered_set<void *> roots_;
    std::unordered_map<void *, Allocation> allocations_;


    GarbageCollector() = default;

    void RemoveRoot(void *ptr);

    void Mark(void *to_mark);

    void Sweep();
public:
    static GarbageCollector& GetInstance() {
        static GarbageCollector instance;
        return instance;
    }
    GarbageCollector(const GarbageCollector&) = delete;
    GarbageCollector& operator=(const GarbageCollector&) = delete;

    void AddAllocation(void *ptr, size_t size);

    void AddAllocation(void *ptr, size_t size, FinalizerT finalizer);

    void AddRoot(void *ptr);

    void DeleteRoot(void *ptr);

    void AddEdge(void *parent, void *child);

    void DeleteEdge(void *parent, void *child);

    void CollectGarbage();

    // FOR TESTING
    size_t GetAllocationsCount();
};

#endif
