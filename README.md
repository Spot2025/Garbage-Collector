# C++ Garbage Collector
Данная библиотека Сборщика мусора для C написана на C++ и использует стандартный алгоритм Mark and Sweep.

Пользователю предоставляются следующие функции, описанные в файле `gc.h`:
```c++
typedef void (*FinalizerT)(void *ptr, size_t size);

void* gc_malloc(size_t size);

void* gc_malloc_manage(size_t size, FinalizerT finalizer);

void gc_delete_root(void *ptr);

void gc_collect();
```

* `gc_malloc` алоцирует указатель нужного размера.
* `gc_malloc_manage` принимает также функцию-деструктор, которая вызовется, когда указатель будет освобожден.
* `gc_delete_root` вычеркивает данный указатель из списка корневых.
* `gc_collect` освобождает не использующиеся указатели.

### Сборка тестов
```
mkdir build
cd build
cmake ..
make BaseTest
./BaseTest
```
