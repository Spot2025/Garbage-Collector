#include "gc_impl.h"

void GarbageCollector::RemoveRoot(void *ptr) {
    allocations_[ptr].count_ = 0;
    roots_.erase(ptr);
}

bool GarbageCollector::PointsTo(void *ptr, Allocation& a) {
    uintptr_t uptr = reinterpret_cast<uintptr_t>(ptr);
    uintptr_t aptr = reinterpret_cast<uintptr_t>(a.ptr_);
    return (uptr >= aptr) && (uptr - aptr <= a.size_);
}

void GarbageCollector::Mark(void *to_mark) {
    Allocation &allocation = allocations_[to_mark];
    allocation.marked_ = true;

    for (uintptr_t ptr = (reinterpret_cast<uintptr_t>(allocation.ptr_) + alignof(void *) - 1) &
        ~(alignof(void *) - 1); ptr - reinterpret_cast<uintptr_t>(allocation.ptr_) <= allocation.size_;
        ptr += alignof(void *)) {
        void *current = *reinterpret_cast<void **>(ptr);
        for (auto& applicant: allocations_) {
            if (PointsTo(current, applicant.second) && !applicant.second.marked_) {
                Mark(applicant.first);
            }
        }
        }
}

void GarbageCollector::Sweep() {
    for (auto it = allocations_.begin(); it != allocations_.end(); ) {
        if (!it->second.marked_) {
            it->second.finalizer_(it->second.ptr_, it->second.size_);
            free(it->second.ptr_);
            it = allocations_.erase(it);
        } else {
            ++it;
        }
    }
}

void GarbageCollector::AddAllocation(void *ptr, size_t size) {
    Allocation allocation{size, ptr, 1, false, DefaultFinalizer};
    allocations_[ptr] = allocation;
}

void GarbageCollector::AddAllocation(void *ptr, size_t size, FinalizerT finalizer) {
    Allocation allocation{size, ptr, 1, false, finalizer};
    allocations_[ptr] = allocation;
}

void GarbageCollector::AddRoot(void *ptr) {
    roots_.insert(ptr);
}

void GarbageCollector::DeleteRoot(void *ptr) {
    if (!roots_.contains(ptr)) {
        throw std::runtime_error("Root not found");
    }
    RemoveRoot(ptr);
}

void GarbageCollector::IncreaseCount(void* ptr) {
    ++allocations_[ptr].count_;
}

void GarbageCollector::DecreaseCount(void* ptr) {
    if (!allocations_.contains(ptr)) {
        throw std::runtime_error("Allocation not found");
    }
    if (--allocations_[ptr].count_ == 0) {
        roots_.erase(ptr);
    }
}

void GarbageCollector::CollectGarbage() {
    for (auto& allocation : allocations_) {
        allocation.second.marked_ = false;
    }
    for (auto root : roots_) {
        Mark(root);
    }
    Sweep();
}

size_t GarbageCollector::GetAllocationsCount() {
    return allocations_.size();
}

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
