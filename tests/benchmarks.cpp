#include <chrono>
#include <iostream>
#include <vector>
#include <gtest/gtest.h>

#include "gc_impl.h"

TEST(PerformanceBenchmark, AllocationAndCollection) {
    const size_t num_objects = 10000;
    std::vector<void*> pointers;

    // Замер времени выделения памяти
    auto allocation_start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_objects; ++i) {
        void* ptr = gc_malloc(sizeof(int));
        gc_add_root(ptr);  // Добавляем корень
        pointers.push_back(ptr);
    }
    auto allocation_end = std::chrono::high_resolution_clock::now();
    auto allocation_time = std::chrono::duration_cast<std::chrono::milliseconds>(allocation_end - allocation_start).count();
    std::cout << "Allocation time for " << num_objects << " objects: " << allocation_time << " ms\n";

    // Замер времени сборки мусора
    auto collection_start = std::chrono::high_resolution_clock::now();
    gc_collect();
    auto collection_end = std::chrono::high_resolution_clock::now();
    auto collection_time = std::chrono::duration_cast<std::chrono::milliseconds>(collection_end - collection_start).count();
    std::cout << "Garbage collection time: " << collection_time << " ms\n";

    // Удаляем корни
    for (size_t i = 0; i < num_objects; ++i) {
        gc_delete_root(pointers[i]);
    }

    // Замер времени сборки мусора после удаления корней
    auto final_collection_start = std::chrono::high_resolution_clock::now();
    gc_collect();
    auto final_collection_end = std::chrono::high_resolution_clock::now();
    auto final_collection_time = std::chrono::duration_cast<std::chrono::milliseconds>(final_collection_end - final_collection_start).count();
    std::cout << "Final garbage collection time: " << final_collection_time << " ms\n";
    std::cout << std::endl;

    // Проверяем, что все объекты удалены
    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 0);
}

TEST(PartialRootDeletionBenchmark, ComplexGraphWithPartialDeletion) {
    struct Node {
        Node* left;
        Node* right;
        int value;
    };

    const size_t num_nodes = 10000;
    std::vector<void*> nodes;

    // Создаем сложный граф объектов
    auto start_create = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_nodes; ++i) {
        void* node = gc_malloc(sizeof(Node));
        gc_add_root(node);  // Добавляем корень
        nodes.push_back(node);
    }

    for (size_t i = 0; i < num_nodes; ++i) {
        Node* current = static_cast<Node*>(nodes[i]);
        Node* left = static_cast<Node*>(nodes[(i * 2 + 1) % num_nodes]);
        Node* right = static_cast<Node*>(nodes[(i * 2 + 2) % num_nodes]);
        new (current) Node{left, right, static_cast<int>(i)};
    }
    auto end_create = std::chrono::high_resolution_clock::now();
    auto create_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_create - start_create).count();
    std::cout << "Graph creation time: " << create_time << " ms\n";

    // Удаляем каждый второй корень
    for (size_t i = 0; i < num_nodes; i += 2) {
        gc_delete_root(nodes[i]);
    }

    // Замер времени сборки мусора
    auto start_collect = std::chrono::high_resolution_clock::now();
    gc_collect();
    auto end_collect = std::chrono::high_resolution_clock::now();
    auto collect_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_collect - start_collect).count();
    std::cout << "Collection time after partial root deletion: " << collect_time << " ms\n";
    std::cout << std::endl;

    // Удаляем оставшиеся корни и завершаем сборку
    for (size_t i = 1; i < num_nodes; i += 2) {
        gc_delete_root(nodes[i]);
    }
    gc_collect();

    // Проверяем, что все объекты удалены
    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 0);
}

TEST(MultipleSmallCyclesBenchmark, ComplexGraphWithSmallCycles) {
    struct Node {
        Node* neighbors[3];
        int value;
    };

    const size_t num_nodes = 10000;
    std::vector<void*> nodes;

    // Создаем граф с множеством небольших циклов
    auto start_create = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_nodes; ++i) {
        void* node = gc_malloc(sizeof(Node));
        gc_add_root(node);  // Добавляем корень
        nodes.push_back(node);
    }

    for (size_t i = 0; i < num_nodes; ++i) {
        Node* current = static_cast<Node*>(nodes[i]);
        for (size_t j = 0; j < 3; ++j) {
            current->neighbors[j] = static_cast<Node*>(nodes[(i + j + 1) % num_nodes]);
        }
        current->value = static_cast<int>(i);
    }
    auto end_create = std::chrono::high_resolution_clock::now();
    auto create_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_create - start_create).count();
    std::cout << "Graph creation time: " << create_time << " ms\n";

    // Замер времени сборки мусора
    auto start_collect = std::chrono::high_resolution_clock::now();
    gc_collect();
    auto end_collect = std::chrono::high_resolution_clock::now();
    auto collect_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_collect - start_collect).count();
    std::cout << "Collection time for graph with multiple small cycles: " << collect_time << " ms\n";
    std::cout << std::endl;

    // Удаляем корни и завершаем сборку
    for (size_t i = 0; i < num_nodes; ++i) {
        gc_delete_root(nodes[i]);
    }
    gc_collect();

    // Проверяем, что все объекты удалены
    EXPECT_EQ(GarbageCollector::GetInstance().GetAllocationsCount(), 0);
}