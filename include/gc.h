#ifndef GC_H
#define GC_H

typedef void (*FinalizerT)(void *ptr, size_t size);

void* gc_malloc(size_t size);
void* gc_malloc_manage(size_t size, FinalizerT finalizer);
void* gc_malloc_root(size_t size);
void* gc_malloc_root_manage(size_t size, FinalizerT finalizer);
void* gc_malloc_with_parent(size_t size, void *parent);
void* gc_malloc_with_parent_manage(size_t size, void *parent, FinalizerT finalizer);
void gc_add_edge(void *parent, void *child);
void gc_del_edge(void *parent, void *child);
void gc_swap_edge(void *parent, void *child1, void *child2);
void gc_add_root(void *ptr);
void gc_delete_root(void *ptr);
void gc_block_collect();
void gc_unlock_collect();
void gc_collect();

void gc_start_incremental_mark();
void gc_step_mark();
bool gc_is_marking();
void gc_finish_incremental_mark();

// Фоновый сборщик
void gc_start_background_collector(size_t steps, int interval_ms);
void gc_stop_background_collector();
bool gc_is_background_collector_running();

#endif //GC_H
