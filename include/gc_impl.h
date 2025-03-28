#ifndef GC_IMPL_H
#define GC_IMPL_H

#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <thread>

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

    std::atomic<bool> incremental_mark_{false};
    std::deque<void*>::iterator mark_iterator_;
    size_t steps_per_increment_{100};

    std::atomic<bool> background_collector_running_{false};
    std::thread background_collector_thread_;
    int background_collector_interval_{100};
    std::condition_variable background_cv_;
    std::mutex background_mutex_;

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

    void AddAllocation(void *ptr, size_t size, FinalizerT finalizer=DefaultFinalizer);
    void AddRootAllocation(void *ptr, size_t size, FinalizerT finalizer=DefaultFinalizer);
    void AddAllocationWithParent(void *ptr, size_t size, void *parent, FinalizerT finalizer=DefaultFinalizer);
    void AddRoot(void *ptr);
    void DeleteRoot(void *ptr);
    void AddEdge(void *parent, void *child);
    void DeleteEdge(void *parent, void *child);
    void SwapEdge(void *parent, void *child1, void *child2);
    void CollectGarbage();
    void BlockCollect();
    void UnlockCollect();

    void StartIncrementalMark();
    void StepMark();
    bool IsMarking() const;
    void FinishIncrementalMark();
    void SetStepsPerIncrement(size_t steps);

    void StartBackgroundCollector(size_t steps, int interval_ms);
    void StopBackgroundCollector();
    bool IsBackgroundCollectorRunning() const;
    void BackgroundCollectorLoop();

    // FOR TESTING
    size_t GetAllocationsCount();
};

#endif
