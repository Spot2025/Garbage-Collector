#include "gc_impl.h"

void GarbageCollector::RemoveRoot(void *ptr) {
    roots_.erase(ptr);
}

void GarbageCollector::Mark(void *to_mark) {
    Allocation &allocation = allocations_[to_mark];
    if (allocation.marked_) {
        return;
    }
    allocation.marked_ = true;

    for (auto edge : allocation.edges) {
        Mark(edge);
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
    Allocation allocation{size, ptr, false, std::unordered_set<void*>(), DefaultFinalizer};
    allocations_[ptr] = allocation;
}

void GarbageCollector::AddAllocation(void *ptr, size_t size, FinalizerT finalizer) {
    Allocation allocation{size, ptr, false, std::unordered_set<void*>(), finalizer};
    allocations_[ptr] = allocation;
}

void GarbageCollector::AddRoot(void *ptr) {
    roots_.insert(ptr);
}

void GarbageCollector::DeleteRoot(void *ptr) {
    RemoveRoot(ptr);
}

void GarbageCollector::AddEdge(void *parent, void *child) {
    allocations_[parent].edges.insert(child);
}

void GarbageCollector::DeleteEdge(void *parent, void *child) {
    allocations_[parent].edges.erase(child);
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
