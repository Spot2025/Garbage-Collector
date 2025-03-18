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
        size_t count_;
        bool marked_;
        FinalizerT finalizer_;
    };

    std::unordered_set<void *> roots_;
    std::unordered_map<void *, Allocation> allocations_;


    GarbageCollector() = default;

    void RemoveRoot(void *ptr);

    bool PointsTo(void *ptr, Allocation& a);

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

    void IncreaseCount(void* ptr);

    void DecreaseCount(void* ptr);

    void CollectGarbage();

    // FOR TESTING
    size_t GetAllocationsCount();
};

#endif
