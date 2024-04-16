#pragma once

#include "common.h"
#include "debug_utils.h"
#include <stdio.h>

typedef struct gcptr GcPtr;

typedef struct gc_objlist {
  /// Nullable.
  GcPtr *items;
  usize len;
  usize cap;
} GcObjlist;

void gc_free_objlist(GcObjlist self);

typedef void (*DestroyCallback)(void *);

#define NO_DESTORY_CALLBACK ((DestroyCallback) nullptr)

/// The metadata attached to a GC object.
typedef struct gc_metadata {
  /// The child objects of this GC object.
  GcObjlist reflist;
  /// The number of references from outside the GC.
  /// Meaning it does not include references that is in the `reflist` of other GC objects.
  /// Object with `strong_count > 0` is effectively the root objects of the arena.
  usize strong_count;
  /// Function to be called when freeing this object.
  /// Null for do nothing.
  /// IMPORTANT:
  /// It is not the responsibility of an object's `destroy_callback` to free its child objects, doing this results in
  /// undefined behavior.
  DestroyCallback destroy_callback;
  /// PRIVATE FIELD.
  /// The `sweep_count` of the arena when this object is last seen alive.
  /// This is the "mark" in "mark & sweep GC".
  usize last_seen_alive;
} GcMetadata;

void gc_free_metadata(GcMetadata);

struct gcptr {
  /// Nullable
  void *obj;
  GcMetadata *metadata;
};

#define OBJLIST_INIT_CAP 32

GcObjlist gc_new_objlist_with_capacity(usize cap);

GcObjlist gc_new_objlist();

void gc_push_objlist(GcObjlist *, GcPtr item);

GcPtr *gc_get_item_objlist(GcObjlist *, usize i);

void gc_free_objlist(GcObjlist);

typedef struct gcarena {
  GcObjlist objects;
  usize sweep_count;
} GcArena;

GcArena gc_new_arena();

void gc_free_arena(GcArena);

void gc_sweep(GcArena *);

/// `value` is the unique pointer to the value on heap.
GcPtr gc_new_object(GcArena *, void *restrict value, GcObjlist reflist, DestroyCallback destroy_callback);

void gc_enters_scope(GcPtr);
void gc_leaves_scope(GcPtr);

#define GC_GET(TY, PTR) ((const TY *)((PTR).obj))

static inline void gc_print_ptr_addr(const GcPtr *);
static inline void gc_print_objlist(const GcObjlist *self) {
  printf("[");
  if (self->len != 0) {
    for (usize i = 0; i < self->len - 1; ++i) {
      gc_print_ptr_addr(&self->items[i]);
      printf(",");
    }
    gc_print_ptr_addr(&self->items[self->len - 1]);
  }
  printf("]");
}

static inline void gc_println_objlist(const GcObjlist *self) {
  gc_print_objlist(self);
  printf("\n");
}

static inline void gc_print_metadata(const GcMetadata *self) {
  printf("metadata(reflist: ");
  gc_print_objlist(&self->reflist);
  printf(", strong_count: %zu, destroy_callback: %p, last_seen_alive: %zu)", self->strong_count, self->destroy_callback,
         self->last_seen_alive);
}

static inline void gc_println_metadata(const GcMetadata *self) {
  gc_print_metadata(self);
  printf("\n");
}

static inline void gc_print_ptr_addr(const GcPtr *self) {
  printf("gcptr(obj: %p, metadata: %p)", self->obj, self->metadata);
}

static inline void gc_println_ptr_addr(const GcPtr *self) {
  gc_print_ptr_addr(self);
  printf("\n");
}

static inline void gc_print_ptr(const GcPtr *self) {
  printf("gcptr(obj: %p, metadata: ", self->obj);
  gc_print_metadata(self->metadata);
  printf(")");
}

static inline void gc_println_ptr(const GcPtr *p) {
  gc_print_ptr(p);
  printf("\n");
}
