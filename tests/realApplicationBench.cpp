#include <benchmark/benchmark.h>
#include <vector>
#include <thread>
#include <random>
#include <atomic>
#include "gc.h"

// Конфигурация тестирования
struct TestConfig {
    int users;
    int edges;
    int threads;
    int gc_frequency_ms; // Частота сборки мусора
    bool dynamic_roots;  // Динамическое изменение корней
};

// Расширенный класс User с дополнительной функциональностью
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

// Функция для создания графа пользователей с заданной конфигурацией
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

// Расширенный benchmark тест
static void BM_ConcurrentGC_Extended(benchmark::State& state) {
    TestConfig config{
        static_cast<int>(state.range(0)), // users
        static_cast<int>(state.range(1)), // edges
        static_cast<int>(state.range(2)), // threads
        static_cast<int>(state.range(3)), // gc_frequency_ms
        static_cast<bool>(state.range(4)) // dynamic_roots
    };

    // Создаем граф пользователей
    std::vector<User*> users;
    createSocialNetwork(config, users);

    // Создаем активных пользователей (корни)
    std::vector<User*> activeUsers;
    gc_block_collect();
    for (int i = 0; i < std::min(100, config.users/10); ++i) {
        User* u = new (gc_malloc_manage(sizeof(User), [](void* ptr, size_t) {
            static_cast<User*>(ptr)->~User();
        })) User(config.users + i);
        activeUsers.push_back(u);
    }
    gc_unlock_collect();

    // Запускаем поток сборщика мусора
    std::atomic<bool> gc_running{true};
    std::thread gc_thread([&]{
        while (gc_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(config.gc_frequency_ms));
            gc_collect();
        }
    });

    // Счетчик для динамического изменения корней
    std::atomic<int> root_change_counter{0};

    for (auto _ : state) {
        // Обрабатываем пользователей в нескольких потоках
        std::vector<std::thread> threads;
        int chunkSize = activeUsers.size() / config.threads;

        for (int i = 0; i < config.threads; ++i) {
            threads.emplace_back([&, i]{
                int start = i * chunkSize;
                int end = (i == config.threads - 1) ? activeUsers.size() : start + chunkSize;

                for (int j = start; j < end; ++j) {
                    activeUsers[j]->process();

                    // Динамическое изменение корней
                    if (config.dynamic_roots && ++root_change_counter % 100 == 0) {
                        int idx = rand() % activeUsers.size();
                        gc_block_collect();
                        gc_delete_root(activeUsers[idx]);

                        User* new_user = new (gc_malloc_manage(sizeof(User), [](void* ptr, size_t) {
                            static_cast<User*>(ptr)->~User();
                        })) User(config.users * 2 + root_change_counter);

                        activeUsers[idx] = new_user;
                        gc_add_root(new_user);
                        gc_unlock_collect();
                    }
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
    }

    // Останавливаем поток GC
    gc_running = false;
    gc_thread.join();

    // Очистка
    for (auto user : activeUsers) {
        gc_delete_root(user);
    }
    // for (auto user : users) {
    //     gc_delete_root(user);
    // }
    gc_collect();

    // Дополнительные метрики
    state.counters["Users"] = config.users;
    state.counters["EdgesPerUser"] = config.edges;
    state.counters["Threads"] = config.threads;
    state.counters["GCFreq_ms"] = config.gc_frequency_ms;
}

// Комплексная конфигурация тестов
BENCHMARK(BM_ConcurrentGC_Extended)
    // Маленький граф, мало потоков, разные частоты GC
    ->Args({10000, 5, 2, 10, 0})    // 1000 пользователей, 5 связей, 2 потока, GC каждые 10мс, статические корни
    ->Args({10000, 5, 2, 50, 0})    // GC каждые 50мс
    ->Args({10000, 5, 2, 100, 0})   // GC каждые 100мс

    // Средний граф, среднее число потоков
    ->Args({50000, 10, 4, 10, 0})
    ->Args({50000, 10, 4, 50, 0})
    ->Args({50000, 10, 4, 100, 0})

    // Большой граф, много потоков
    ->Args({100000, 15, 8, 10, 0})
    ->Args({100000, 15, 8, 50, 0})
    ->Args({100000, 15, 8, 100, 0})

    // Те же конфигурации с динамическими корнями
    ->Args({10000, 5, 2, 10, 1})
    ->Args({50000, 10, 4, 10, 1})
    ->Args({100000, 15, 8, 10, 1})

    // Экстремальные случаи
    ->Args({200000, 20, 16, 5, 0})  // Очень большой граф, много потоков, частый GC
    ->Args({200000, 20, 16, 50, 1}) // + динамические корни
    ->Unit(benchmark::kMillisecond)
    ->MeasureProcessCPUTime()
    ->UseRealTime();

BENCHMARK_MAIN();