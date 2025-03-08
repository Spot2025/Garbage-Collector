#ifndef GC_H
#define GC_H

typedef void (*FinalizerT)(void *ptr, size_t size);

void* gc_malloc(size_t size);

void* gc_malloc_manage(size_t size, FinalizerT finalizer);

#endif //GC_H
