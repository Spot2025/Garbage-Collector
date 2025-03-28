#include <benchmark/benchmark.h>
#include <vector>
#include <thread>
#include <random>
#include <atomic>
#include "gc.h"

struct TestConfig {
    int users;
    int edges;
    int threads;
    int steps;
    int gc_interval_ms;
    bool use_incremental;
};

class User {
public:
    int id;
    std::vector<User*> friends;
    std::atomic<int> activity{0};

    User(int userId) : id(userId) {
        gc_add_root(this);
    }

    ~User() {
        // Финализатор
    }

    void addFriend(User* friendUser) {
        friends.push_back(friendUser);
        gc_add_edge(this, friendUser);
    }

    void process() {
        // Имитация обработки с переменной нагрузкой
        for (volatile int i = 0; i < 50 + (id % 100); ++i) {}
        activity++;
    }
};

static void createSocialNetwork(const TestConfig& config, std::vector<User*>& users) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, config.users-1);

    users.resize(config.users);

    gc_block_collect();

    for (int i = 0; i < config.users; ++i) {
        users[i] = new (gc_malloc_manage(sizeof(User), [](void* ptr, size_t) {
            static_cast<User*>(ptr)->~User();
        })) User(i);
    }

    for (int i = 0; i < config.users; ++i) {
        for (int j = 0; j < config.edges; ++j) {
            int friendId = dis(gen);
            if (friendId != i) {
                users[i]->addFriend(users[friendId]);
            }
        }
    }

    gc_unlock_collect();

    for (int i = 0; i < config.users; ++i) {
        gc_delete_root(users[i]);
    }
}

static void BM_StandardGC(benchmark::State& state) {
    TestConfig config{
        static_cast<int>(state.range(0)), // users
        static_cast<int>(state.range(1)), // edges
        static_cast<int>(state.range(2)), // threads
        0,  // gc_interval_ms - не используется
        false // use_incremental
    };
    
    std::vector<User*> users;
    createSocialNetwork(config, users);

    gc_block_collect();
    std::vector<User*> activeUsers;
    for (int i = 0; i < std::min(100, config.users/10); ++i) {
        User* u = new (gc_malloc_manage(sizeof(User), [](void* ptr, size_t) {
            static_cast<User*>(ptr)->~User();
        })) User(config.users + i);
        activeUsers.push_back(u);
    }
    gc_unlock_collect();

    for (auto _ : state) {
        std::vector<std::thread> threads;
        int chunkSize = activeUsers.size() / config.threads;

        for (int i = 0; i < config.threads; ++i) {
            threads.emplace_back([&, i]{
                int start = i * chunkSize;
                int end = (i == config.threads - 1) ? activeUsers.size() : start + chunkSize;
                for (int j = start; j < end; ++j) {
                    activeUsers[j]->process();
                }
            });
        }

        // Полная сборка мусора между итерациями
        gc_collect();

        for (auto& t : threads) {
            t.join();
        }
    }

    for (auto user : activeUsers) {
        gc_delete_root(user);
    }
    gc_collect();
}

static void BM_IncrementalGC(benchmark::State& state) {
    TestConfig config{
        static_cast<int>(state.range(0)), // users
        static_cast<int>(state.range(1)), // edges
        static_cast<int>(state.range(2)), // threads
        static_cast<int>(state.range(3)), // steps
        static_cast<int>(state.range(4)), // gc_interval_ms
        true // use_incremental
    };
    
    std::vector<User*> users;
    createSocialNetwork(config, users);

    gc_block_collect();
    std::vector<User*> activeUsers;
    for (int i = 0; i < std::min(100, config.users/10); ++i) {
        User* u = new (gc_malloc_manage(sizeof(User), [](void* ptr, size_t) {
            static_cast<User*>(ptr)->~User();
        })) User(config.users + i);
        activeUsers.push_back(u);
    }
    gc_unlock_collect();

    // Запускаем фоновый инкрементальный сборщик
    gc_start_background_collector(config.steps, config.gc_interval_ms);

    for (auto _ : state) {
        std::vector<std::thread> threads;
        int chunkSize = activeUsers.size() / config.threads;

        for (int i = 0; i < config.threads; ++i) {
            threads.emplace_back([&, i]{
                int start = i * chunkSize;
                int end = (i == config.threads - 1) ? activeUsers.size() : start + chunkSize;
                for (int j = start; j < end; ++j) {
                    activeUsers[j]->process();
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    gc_stop_background_collector();

    for (auto user : activeUsers) {
        gc_delete_root(user);
    }
    gc_collect();
}

// Регистрация бенчмарков
BENCHMARK(BM_StandardGC)
    ->Args({10000, 5, 2})
    ->Args({50000, 10, 4})
    ->Args({100000, 15, 8})
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->UseRealTime()
    ->Name("StandardGC");

BENCHMARK(BM_IncrementalGC)
    ->Args({10000, 5, 2, 100, 10})
    ->Args({50000, 10, 4, 100, 10})
    ->Args({100000, 15, 8, 100, 10})
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->UseRealTime()
    ->Name("IncrementalGC");

BENCHMARK_MAIN();