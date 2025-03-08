#include <gtest/gtest.h>

#include "gc_impl.h"
#include <iostream>

void TestFinalizer(void *ptr, size_t size) {
    std::cout << "Finalizer called for ptr: " << ptr << ", size: " << size << std::endl;
}

TEST(AllocTest, BasicAlloc) {
    void* ptr1 = gc_malloc(sizeof(int));
    void* ptr2 = gc_malloc(sizeof(double));

    gc_collect();

    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 2);

    gc_delete_root(ptr1);
    gc_delete_root(ptr2);

    gc_collect();
    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 0);
}

class TestClass {
public:
    int value;

    TestClass(int v) : value(v) {}
};

TEST(FinalizerTest, FinalizerFunctionality) {
    void* ptr = gc_malloc_manage(sizeof(TestClass), TestFinalizer);
    new (ptr) TestClass(42); // Размещаем объект в выделенной памяти

    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 1);

    gc_delete_root(ptr);
    gc_collect();

    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 0);
}

TEST(GCChainTest, GarbageCollectionWithChains) {
    struct Node {
        Node* next;
        int value;
    };

    void* node1 = gc_malloc(sizeof(Node));
    void* node2 = gc_malloc(sizeof(Node));
    void* node3 = gc_malloc(sizeof(Node));

    new (node1) Node{reinterpret_cast<Node*>(node2), 1};
    new (node2) Node{reinterpret_cast<Node*>(node3), 2};
    new (node3) Node{nullptr, 3};

    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 3);

    gc_delete_root(node2);
    gc_delete_root(node3);

    gc_collect();
    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 3);

    gc_delete_root(node1);
    gc_collect();
    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 0);
}

TEST(GCCyclycTest, GarbageCollectionWithCycles) {
    struct Node {
        Node* next;
        int value;
    };

    void* node1 = gc_malloc(sizeof(Node));
    void* node2 = gc_malloc(sizeof(Node));
    void* node3 = gc_malloc(sizeof(Node));

    new (node1) Node{reinterpret_cast<Node*>(node2), 1};
    new (node2) Node{reinterpret_cast<Node*>(node3), 2};
    new (node3) Node{reinterpret_cast<Node*>(node1), 3};

    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 3);

    gc_delete_root(node2);
    gc_delete_root(node3);

    gc_collect();
    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 3);

    gc_delete_root(node1);
    gc_collect();
    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 0);
}