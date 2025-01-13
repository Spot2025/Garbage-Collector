#ifndef GC_H
#define GC_H

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

class GarbageCollector {
    struct Allocation {
        size_t size_;
        void *ptr_;
        size_t count_;
        bool marked_;
        std::function<void()> destructor_;
    };

    std::unordered_set<void *> roots_;
    std::unordered_map<void *, Allocation> allocations_;


    GarbageCollector() = default;

    void RemoveRoot(void *ptr) {
        allocations_[ptr].count_ = 0;
        roots_.erase(ptr);
    }

    bool PointsTo(void *ptr, Allocation& a) {
        uintptr_t uptr = reinterpret_cast<uintptr_t>(ptr);
        uintptr_t aptr = reinterpret_cast<uintptr_t>(a.ptr_);
        return (uptr >= aptr) && (uptr - aptr <= a.size_);
    }

    void Mark(void *to_mark) {
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

    void Sweep() {
        for (auto it = allocations_.begin(); it != allocations_.end(); ) {
            if (!it->second.marked_) {
                it->second.destructor_();
                it = allocations_.erase(it);
            } else {
                ++it;
            }
        }
    }
public:
    static GarbageCollector& GetInstance() {
        static GarbageCollector instance;
        return instance;
    }
    GarbageCollector(const GarbageCollector&) = delete;
    GarbageCollector& operator=(const GarbageCollector&) = delete;

    template <class T>
    void AddAllocation(T *ptr) {
        Allocation allocation{sizeof(T), ptr, 1, false,
        [ptr]() {
            delete ptr;
        }};
        allocations_[ptr] = allocation;
    }

    template <class T>
    void AddAllocation(T *ptr, size_t size) {
        Allocation allocation{sizeof(T) * size,  ptr, 1, false,
            [ptr]() {
                delete[] ptr;
            }};
        allocations_[ptr] = allocation;
    }

    template <class T>
    void AddRoot(T *ptr) {
        roots_.insert(ptr);
    }

    template <class T>
    void DeleteRoot(T *ptr) {
        if (!roots_.contains(ptr)) {
            throw std::runtime_error("Root not found");
        }
        RemoveRoot(ptr);
    }

    template <class T>
    void IncreaseCount(T* ptr) {
        ++allocations_[ptr].count_;
    }

    template <class T>
    void DecreaseCount(T* ptr) {
        if (!allocations_.contains(ptr)) {
            throw std::runtime_error("Allocation not found");
        }
        if (--allocations_[ptr].count_ == 0) {
            roots_.erase(ptr);
        }
    }

    void CollectGarbage() {
        for (auto& allocation : allocations_) {
            allocation.second.marked_ = false;
        }
        for (auto root : roots_) {
            Mark(root);
        }
        Sweep();
    }
};


template <class T>
class GcPtr {
    T* ptr_ = nullptr;
    GcPtr(T* ptr) : ptr_(ptr) {}

    template <class Y, class... Args>
    friend GcPtr<Y> GcNew(Args&&... args);
public:
    GcPtr() = default;

    GcPtr(const GcPtr<T>& other) : ptr_(other.ptr_) {
        GarbageCollector::GetInstance().IncreaseCount(ptr_);
    }

    template <class Y>
    GcPtr(const GcPtr<Y>& other) : ptr_(other.ptr_) {
        GarbageCollector::GetInstance().IncreaseCount(ptr_);
    }

    GcPtr(const GcPtr<T>&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    template <class Y>
    GcPtr(const GcPtr<Y>&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    GcPtr<T>& operator=(const GcPtr<T>& other) {
        if (ptr_ == other.ptr_) {
            return *this;
        }
        if (ptr_) {
            GarbageCollector::GetInstance().DecreaseCount(ptr_);
        }
        ptr_ = other.ptr_;
        GarbageCollector::GetInstance().IncreaseCount(ptr_);

        return *this;
    }

    template <class Y>
    GcPtr<T>& operator=(const GcPtr<Y>& other) {
        if (ptr_ == other.ptr_) {
            return *this;
        }
        if (ptr_) {
            GarbageCollector::GetInstance().DecreaseCount(ptr_);
        }
        ptr_ = other.ptr_;
        GarbageCollector::GetInstance().IncreaseCount(ptr_);

        return *this;
    }

    GcPtr<T>& operator=(const GcPtr<T>&& other) noexcept {
        if (ptr_ == other.ptr_) {
            return *this;
        }
        if (ptr_) {
            GarbageCollector::GetInstance().DecreaseCount(ptr_);
        }
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;

        return *this;
    }

    template <class Y>
    GcPtr<T>& operator=(const GcPtr<Y>&& other) noexcept {
        if (ptr_ == other.ptr_) {
            return *this;
        }
        if (ptr_) {
            GarbageCollector::GetInstance().DecreaseCount(ptr_);
        }
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;

        return *this;
    }

    T& operator*() const {
        return *ptr_;
    }

    T* operator->() const {
        return ptr_;
    }

    void Reset() {
        if (!ptr_) {
            return;
        }
        GarbageCollector::GetInstance().DecreaseCount(ptr_);
        ptr_ = nullptr;
    }

    ~GcPtr() {
        if (!ptr_) {
            return;
        }
        GarbageCollector::GetInstance().DecreaseCount(ptr_);
    }
};


template <class T>
class GcPtr<T[]> {
    T* ptr_ = nullptr;
    GcPtr(T* ptr) : ptr_(ptr) {}

    template <class Y>
    friend GcPtr<Y[]> GcNewArr(size_t size);
public:
    GcPtr() = default;

    GcPtr(const GcPtr<T>& other) : ptr_(other.ptr_) {
        GarbageCollector::GetInstance().IncreaseCount(ptr_);
    }

    template <class Y>
    GcPtr(const GcPtr<Y>& other) : ptr_(other.ptr_) {
        GarbageCollector::GetInstance().IncreaseCount(ptr_);
    }

    GcPtr(const GcPtr<T>&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    template <class Y>
    GcPtr(const GcPtr<Y>&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    GcPtr<T>& operator=(const GcPtr<T>& other) {
        if (ptr_ == other.ptr_) {
            return *this;
        }
        if (ptr_) {
            GarbageCollector::GetInstance().DecreaseCount(ptr_);
        }
        ptr_ = other.ptr_;
        GarbageCollector::GetInstance().IncreaseCount(ptr_);

        return *this;
    }

    template <class Y>
    GcPtr<T>& operator=(const GcPtr<Y>& other) {
        if (ptr_ == other.ptr_) {
            return *this;
        }
        if (ptr_) {
            GarbageCollector::GetInstance().DecreaseCount(ptr_);
        }
        ptr_ = other.ptr_;
        GarbageCollector::GetInstance().IncreaseCount(ptr_);

        return *this;
    }

    GcPtr<T>& operator=(const GcPtr<T>&& other) noexcept {
        if (ptr_ == other.ptr_) {
            return *this;
        }
        if (ptr_) {
            GarbageCollector::GetInstance().DecreaseCount(ptr_);
        }
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;

        return *this;
    }

    template <class Y>
    GcPtr<T>& operator=(const GcPtr<Y>&& other) noexcept {
        if (ptr_ == other.ptr_) {
            return *this;
        }
        if (ptr_) {
            GarbageCollector::GetInstance().DecreaseCount(ptr_);
        }
        ptr_ = other.ptr_;
        other.ptr_ = nullptr;

        return *this;
    }

    T& operator*() const {
        return *ptr_;
    }

    T* operator->() const {
        return ptr_;
    }

    T& operator[](size_t i) const {
        return ptr_[i];
    }

    void Reset() {
        if (!ptr_) {
            return;
        }
        GarbageCollector::GetInstance().DecreaseCount(ptr_);
        ptr_ = nullptr;
    }

    ~GcPtr() {
        if (!ptr_) {
            return;
        }
        GarbageCollector::GetInstance().DecreaseCount(ptr_);
    }
};

template <class T, class... Args>
GcPtr<T> GcNew(Args&&... args) {
    T* ptr = new T(std::forward<Args>(args)...);
    GarbageCollector::GetInstance().AddAllocation(ptr);
    GarbageCollector::GetInstance().AddRoot(ptr);
    return GcPtr<T>(ptr);
}

template <class T>
GcPtr<T[]> GcNewArr(size_t size) {
    T* ptr = new T[size];
    GarbageCollector::GetInstance().AddAllocation(ptr, size);
    GarbageCollector::GetInstance().AddRoot(ptr);
    return GcPtr<T[]>(ptr);
}

#endif
