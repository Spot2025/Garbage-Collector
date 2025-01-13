#include <gtest/gtest.h>

#include "gc.h"
#include <iostream>

std::string tester;

struct A {
    A() {
        tester += "A() ";
    }
    ~A() {
        tester += "~A()";
    }
};

TEST(AllocTest, Collect) {
    {
        auto ptr = GcNew<A>();
        tester += "| ";
        GarbageCollector::GetInstance().CollectGarbage();
    }
    GarbageCollector::GetInstance().CollectGarbage();
    ASSERT_EQ(tester, "A() | ~A()");
}

TEST(AllocTest, TripleCollect) {
    tester.clear();
    {
        auto ptr1 = GcNew<A>();
        auto ptr2 = GcNew<A>();
        auto ptr3 = GcNew<A>();
        tester += "| ";
        GarbageCollector::GetInstance().CollectGarbage();
    }
    GarbageCollector::GetInstance().CollectGarbage();
    ASSERT_EQ(tester, "A() A() A() | ~A()~A()~A()");
}

TEST(AllocTest, Array) {
    {
        auto arr = GcNewArr<int>(5);
        for (int i = 0; i < 5; ++i) {
            arr[i] = i * 10;
        }
        for (int i = 0; i < 5; ++i) {
            ASSERT_EQ(arr[i], i * 10);
        }
    }
    GarbageCollector::GetInstance().CollectGarbage();
}

TEST(AllocTest, Reset) {
    tester.clear();
    {
        auto ptr1 = GcNew<A>();
        auto ptr2 = GcNew<A>();

        ptr1.Reset();
        GarbageCollector::GetInstance().CollectGarbage();
        tester += " | ";
    }
    GarbageCollector::GetInstance().CollectGarbage();
    ASSERT_EQ(tester, "A() A() ~A() | ~A()");
}

struct Node {
    GcPtr<Node> next;
    Node() {
        tester += "Node()";
    }
    ~Node() {
        tester += "~Node()";
    }
};

TEST(AllocTest, Cyclyc) {
    {
        auto node1 = GcNew<Node>();
        auto node2 = GcNew<Node>();

        node1->next = node2;
        node2->next = node1;
    }
    GarbageCollector::GetInstance().CollectGarbage();
    ASSERT_EQ(tester, "Node()Node()~Node()~Node()");
}