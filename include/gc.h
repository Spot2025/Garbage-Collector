#ifndef GC_H
#define GC_H

typedef void (*FinalizerT)(void *ptr, size_t size);

void* gc_malloc(size_t size);

void* gc_malloc_manage(size_t size, FinalizerT finalizer);

void gc_add_root(void *ptr);

void gc_delete_root(void *ptr);

void gc_collect();

#endif //GC_H
