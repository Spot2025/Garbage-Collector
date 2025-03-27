#include "gc_impl.h"

void GarbageCollector::RemoveRoot(void *ptr) {
    std::unique_lock<std::shared_mutex> lock(roots_mutex_);
    roots_.erase(ptr);
}

void GarbageCollector::Mark() {
    {
        std::unique_lock<std::mutex> gray_lock(gray_mutex_);
        std::unique_lock<std::shared_mutex> lock(roots_mutex_);
        for (auto root : roots_) {
            allocations_[root].color_ = Color::Gray;
            gray_objects_.push_back(root);
        }
    }

    while (!gray_objects_.empty()) {
        void* current = gray_objects_.front();
        gray_objects_.pop_front();

        auto it = allocations_.find(current);
        for (auto ref : it->second.edges) {
            auto edge_it = allocations_.find(ref);
            if (edge_it->second.color_ == Color::White) {
                edge_it->second.color_ = Color::Gray;
                gray_objects_.push_back(ref);
            }
        }
        it->second.color_ = Color::Black;
    }

    while (true) {
        void* current = nullptr;

        {
            std::unique_lock<std::mutex> gray_lock(gray_mutex_);
            if (gray_objects_.empty()) break;
            current = gray_objects_.front();
            gray_objects_.pop_front();
        }

        {
            Allocation* alloc = nullptr;
            std::shared_lock<std::shared_mutex> alloc_lock(allocations_mutex_);
            auto it = allocations_.find(current);
            if (it == allocations_.end()) continue;
            alloc = &it->second;

            std::unique_lock<std::mutex> gray_lock(gray_mutex_);
            for (auto ref : alloc->edges) {
                std::shared_lock<std::shared_mutex> alloc_lock(allocations_mutex_);
                auto edge_it = allocations_.find(ref);
                if (edge_it->second.color_ == Color::White) {
                    edge_it->second.color_ = Color::Gray;
                    gray_objects_.push_back(ref);
                }
            }

            alloc->color_ = Color::Black;
        }
    }
}

void GarbageCollector::Sweep() {
    std::unique_lock<std::shared_mutex> alloc_lock(allocations_mutex_);
    for (auto it = allocations_.begin(); it != allocations_.end(); ) {
        if (it->second.color_ == Color::White) {
            if (it->second.ptr_) {
                it->second.finalizer_(it->second.ptr_, it->second.size_);
                free(it->second.ptr_);
                it->second.ptr_ = nullptr;
            }
            it = allocations_.erase(it);
        } else {
            //it->second.color_ = Color::White;
            ++it;
        }
    }
}

void GarbageCollector::AddAllocation(void *ptr, size_t size) {
    std::unique_lock<std::shared_mutex> lock(allocations_mutex_);
    allocations_[ptr] = {size, ptr, Color::White, {}, DefaultFinalizer};
}

void GarbageCollector::AddAllocation(void *ptr, size_t size, FinalizerT finalizer) {
    std::unique_lock<std::shared_mutex> lock(allocations_mutex_);
    allocations_[ptr] = {size, ptr, Color::White, {}, finalizer};
}

void GarbageCollector::AddRoot(void *ptr) {
    std::unique_lock<std::shared_mutex> lock(roots_mutex_);
    roots_.insert(ptr);
}

void GarbageCollector::DeleteRoot(void *ptr) {
    RemoveRoot(ptr);
}

void GarbageCollector::AddEdge(void *parent, void *child) {
    std::shared_lock<std::shared_mutex> alloc_lock(allocations_mutex_);
    auto parent_it = allocations_.find(parent);
    auto child_it = allocations_.find(child);
    parent_it->second.edges.insert(child);

    if (gc_in_progress_.load() &&
        parent_it->second.color_ == Color::Black &&
        child_it->second.color_ == Color::White) {
        child_it->second.color_ = Color::Gray;
        std::unique_lock<std::mutex> gray_lock(gray_mutex_);
        gray_objects_.push_back(child);
        }
}

void GarbageCollector::DeleteEdge(void *parent, void *child) {
    std::shared_lock<std::shared_mutex> alloc_lock(allocations_mutex_);
    allocations_[parent].edges.erase(child);
}

void GarbageCollector::CollectGarbage() {
    std::unique_lock<std::shared_mutex> gc_lock(gc_mutex_);
    gc_in_progress_.store(true);

    {
        std::shared_lock<std::shared_mutex> alloc_lock(allocations_mutex_);
        for (auto& allocation : allocations_) {
            allocation.second.color_ = Color::White;
        }
    }

    Mark();
    Sweep();

    gc_in_progress_.store(false);
}

size_t GarbageCollector::GetAllocationsCount() {
    std::shared_lock<std::shared_mutex> lock(allocations_mutex_);
    return allocations_.size();
}

void GarbageCollector::StartIncrementalMark() {
    std::unique_lock<std::shared_mutex> gc_lock(gc_mutex_);
    incremental_mark_.store(true);
    gc_in_progress_.store(true);

    {
        std::shared_lock<std::shared_mutex> alloc_lock(allocations_mutex_);
        for (auto& allocation : allocations_) {
            allocation.second.color_ = Color::White;
        }
    }

    {
        std::unique_lock<std::mutex> gray_lock(gray_mutex_);
        std::shared_lock<std::shared_mutex> roots_lock(roots_mutex_);
        gray_objects_.clear();
        for (auto root : roots_) {
            allocations_[root].color_ = Color::Gray;
            gray_objects_.push_back(root);
        }
        mark_iterator_ = gray_objects_.begin();
    }
}

void GarbageCollector::StepMark() {
    if (!gc_in_progress_.load() || !incremental_mark_.load()) return;

    std::shared_lock<std::shared_mutex> alloc_lock(allocations_mutex_);
    std::unique_lock<std::mutex> gray_lock(gray_mutex_);

    size_t processed = 0;
    while (mark_iterator_ != gray_objects_.end() && processed < steps_per_increment_) {
        void* current = *mark_iterator_;
        if (current) {
            auto it = allocations_.find(current);
            if (it != allocations_.end()) {
                for (auto ref : it->second.edges) {
                    auto edge_it = allocations_.find(ref);
                    if (edge_it->second.color_ == Color::White) {
                        edge_it->second.color_ = Color::Gray;
                        gray_objects_.push_back(ref);
                    }
                }
                it->second.color_ = Color::Black;
            }
        }
        ++mark_iterator_;
        ++processed;
    }

    if (mark_iterator_ == gray_objects_.end()) {
        FinishIncrementalMark();
        Sweep();
        gc_in_progress_.store(false);
    }
}

bool GarbageCollector::IsMarking() const {
    return gc_in_progress_.load() && incremental_mark_.load();
}

void GarbageCollector::FinishIncrementalMark() {
    std::unique_lock<std::shared_mutex> gc_lock(gc_mutex_);
    incremental_mark_ = false;
}

void GarbageCollector::StartBackgroundCollector(size_t steps, int interval_ms) {
    if (background_collector_running_) return;

    steps_per_increment_ = steps;
    background_collector_interval_ = interval_ms;
    background_collector_running_ = true;
    background_collector_thread_ = std::thread(&GarbageCollector::BackgroundCollectorLoop, this);
}

void GarbageCollector::StopBackgroundCollector() {
    if (!background_collector_running_.exchange(false)) {
        return;
    }

    background_cv_.notify_all();

    if (background_collector_thread_.joinable()) {
        background_collector_thread_.join();
    }
}

bool GarbageCollector::IsBackgroundCollectorRunning() const {
    return background_collector_running_.load();
}

void GarbageCollector::BackgroundCollectorLoop() {
    while (background_collector_running_) {
        {
            std::unique_lock<std::mutex> lock(background_mutex_);
            background_cv_.wait_for(lock,
                std::chrono::milliseconds(background_collector_interval_),
                [this] { return !background_collector_running_; });

            if (!background_collector_running_) {
                break;
            }
        }

        if (!gc_in_progress_.load()) {
            StartIncrementalMark();
        }

        StepMark();
    }
}