#ifndef GC_IMPL_H
#define GC_IMPL_H

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <cstdlib>

#include "gc.h"

inline void DefaultFinalizer(void *ptr, size_t size) {
    return;
}

enum class Color {
    White, Gray, Black
};

class GarbageCollector {
    struct Allocation {
        size_t size_;
        void *ptr_;
        Color color_;
        std::unordered_set<void*> edges;
        FinalizerT finalizer_;
    };

    std::unordered_set<void *> roots_;
    std::shared_mutex roots_mutex_;

    std::unordered_map<void *, Allocation> allocations_;
    std::shared_mutex allocations_mutex_;

    std::deque<void*> gray_objects_;
    std::mutex gray_mutex_;

    std::atomic<bool> gc_in_progress_{false};
    std::shared_mutex gc_mutex_;

    GarbageCollector() = default;
    void RemoveRoot(void *ptr);
    void Mark();
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
