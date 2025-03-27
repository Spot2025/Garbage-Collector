#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include "gc.h"

// Простые структуры для тестирования
struct Node {
    std::vector<Node*> children;
    int value;
};

struct LargeObject {
    char data[1024 * 1024]; // 1MB объект
    int id;
};

// Финализаторы для управляемых объектов
void node_finalizer(void* ptr, size_t size) {
    Node* node = static_cast<Node*>(ptr);
    // Освобождение ресурсов, если необходимо
}

void large_object_finalizer(void* ptr, size_t size) {
    LargeObject* obj = static_cast<LargeObject*>(ptr);
    // Освобождение ресурсов, если необходимо
}

// Бенчмарк: создание и удаление большого количества малых объектов
static void BM_SmallObjectsAllocation(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        gc_collect(); // Очистка перед каждым запуском
        state.ResumeTiming();

        const int count = state.range(0);
        std::vector<void*> objects;
        objects.reserve(count);

        for (int i = 0; i < count; i++) {
            void* ptr = gc_malloc(sizeof(int));
            objects.push_back(ptr);
            if (i % 2 == 0) { // Половину объектов делаем достижимыми
                gc_add_root(ptr);
            }
        }

        gc_collect();

        // Удаление корней
        for (int i = 0; i < count; i += 2) {
            gc_delete_root(objects[i]);
        }
    }
}

// Бенчмарк: создание древовидной структуры данных
static void BM_TreeStructure(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        gc_collect(); // Очистка перед каждым запуском
        state.ResumeTiming();

        const int depth = state.range(0);
        const int branching = state.range(1);

        // Создание корня дерева
        Node* root = static_cast<Node*>(gc_malloc_manage(sizeof(Node), node_finalizer));
        new (root) Node;
        root->value = 0;
        gc_add_root(root);

        // Очередь для построения дерева
        std::vector<Node*> queue;
        queue.push_back(root);

        int current_depth = 0;
        int nodes_at_current_level = 1;
        int nodes_at_next_level = 0;

        while (current_depth < depth && !queue.empty()) {
            Node* current = queue.front();
            queue.erase(queue.begin());

            // Создаем потомков
            for (int i = 0; i < branching; i++) {
                Node* child = static_cast<Node*>(gc_malloc_manage(sizeof(Node), node_finalizer));
                new (child) Node();
                child->value = current->value + 1;
                current->children.push_back(child);
                gc_add_edge(current, child);

                if (current_depth < depth - 1) {
                    queue.push_back(child);
                    nodes_at_next_level++;
                }
            }

            nodes_at_current_level--;
            if (nodes_at_current_level == 0) {
                current_depth++;
                nodes_at_current_level = nodes_at_next_level;
                nodes_at_next_level = 0;
            }
        }

        // Запускаем сборку мусора
        gc_collect();

        // Удаляем корень
        gc_delete_root(root);
    }
}

// Бенчмарк: создание циклических структур
static void BM_CyclicStructures(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        gc_collect(); // Очистка перед каждым запуском
        state.ResumeTiming();

        const int num_clusters = state.range(0);
        const int nodes_per_cluster = state.range(1);

        std::vector<Node*> roots;

        for (int c = 0; c < num_clusters; c++) {
            // Создаем кластер узлов с циклическими ссылками
            std::vector<Node*> cluster;

            // Создаем узлы
            for (int i = 0; i < nodes_per_cluster; i++) {
                Node* node = static_cast<Node*>(gc_malloc_manage(sizeof(Node), node_finalizer));
                new (node) Node();
                node->value = c * nodes_per_cluster + i;
                cluster.push_back(node);

                if (i == 0) {
                    // Первый узел в кластере делаем корневым
                    gc_add_root(node);
                    roots.push_back(node);
                }
            }

            // Создаем циклические связи
            for (int i = 0; i < nodes_per_cluster; i++) {
                for (int j = 0; j < nodes_per_cluster; j++) {
                    if (i != j) {
                        cluster[i]->children.push_back(cluster[j]);
                        gc_add_edge(cluster[i], cluster[j]);
                    }
                }
            }
        }

        // Удаляем половину корней, чтобы создать недостижимые циклы
        for (int i = 0; i < num_clusters / 2; i++) {
            gc_delete_root(roots[i]);
        }

        gc_collect();

        // Удаляем оставшиеся корни
        for (int i = num_clusters / 2; i < num_clusters; i++) {
            gc_delete_root(roots[i]);
        }
    }
}

// Бенчмарк: работа с большими объектами
static void BM_LargeObjects(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        gc_collect(); // Очистка перед каждым запуском
        state.ResumeTiming();

        const int count = state.range(0);
        std::vector<LargeObject*> large_objects;

        for (int i = 0; i < count; i++) {
            LargeObject* obj = static_cast<LargeObject*>(
                gc_malloc_manage(sizeof(LargeObject), large_object_finalizer));
            obj->id = i;
            large_objects.push_back(obj);

            if (i % 3 == 0) { // Треть объектов делаем корневыми
                gc_add_root(obj);
            }
        }

        gc_collect();

        // Удаляем корни
        for (int i = 0; i < count; i += 3) {
            gc_delete_root(large_objects[i]);
        }
    }
}

// Бенчмарк: интенсивная работа со ссылками (добавление/удаление)
static void BM_ReferenceIntensive(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        gc_collect(); // Очистка перед каждым запуском
        state.ResumeTiming();

        const int num_objects = state.range(0);
        const int num_operations = state.range(1);

        // Создаем объекты
        std::vector<Node*> objects;
        objects.reserve(num_objects);

        for (int i = 0; i < num_objects; i++) {
            Node* node = static_cast<Node*>(gc_malloc_manage(sizeof(Node), node_finalizer));
            new (node) Node();
            node->value = i;
            objects.push_back(node);

            // 10% объектов делаем корневыми
            if (i % 10 == 0) {
                gc_add_root(node);
            }
        }

        // Генератор случайных чисел
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distr(0, num_objects - 1);

        // Выполняем случайные операции добавления/удаления ссылок
        for (int op = 0; op < num_operations; op++) {
            int src_idx = distr(gen);
            int dst_idx = distr(gen);

            if (src_idx != dst_idx) {
                if (op % 2 == 0) {
                    // Добавляем ссылку
                    objects[src_idx]->children.push_back(objects[dst_idx]);
                    gc_add_edge(objects[src_idx], objects[dst_idx]);
                } else if (!objects[src_idx]->children.empty()) {
                    // Удаляем ссылку
                    Node* child = objects[src_idx]->children.back();
                    objects[src_idx]->children.pop_back();
                    gc_del_edge(objects[src_idx], child);
                }
            }

            if (op % 100 == 0) {
                gc_collect(); // Периодически запускаем сборку мусора
            }
        }

        // Удаляем все корни
        for (int i = 0; i < num_objects; i += 10) {
            gc_delete_root(objects[i]);
        }

        gc_collect();
    }
}

// Регистрация бенчмарков
BENCHMARK(BM_SmallObjectsAllocation)->Args({1000})->Args({10000})->Args({100000});
BENCHMARK(BM_TreeStructure)->Args({3, 3})->Args({5, 2})->Args({7, 2});
BENCHMARK(BM_CyclicStructures)->Args({10, 5})->Args({20, 10})->Args({50, 20});
BENCHMARK(BM_LargeObjects)->Args({10})->Args({50})->Args({100});
BENCHMARK(BM_ReferenceIntensive)->Args({100, 1000})->Args({1000, 10000});

// Основная функция для запуска бенчмарков
int main(int argc, char** argv) {
    benchmark::Initialize(&argc, argv);
    benchmark::SetDefaultTimeUnit(benchmark::kMillisecond);
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}

