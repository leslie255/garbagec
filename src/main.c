#include "common.h"
#include "debug_utils.h"
#include <assert.h>
#include <complex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gcptr GcPtr;

/// Immutable length heap array of booleans.
typedef struct boolarray {
  bool *items;
  usize len;
} BoolArray;

static inline BoolArray new_boolarray(usize len) {
  bool *items = xalloc(bool, len);
  bzero(items, len);
  return (BoolArray){.items = items, .len = len};
}

static inline bool *get_item_boolarray(BoolArray *self, usize i) {
  DEBUG_ASSERT(self->items != nullptr);
  DEBUG_ASSERT(i < self->len);
  return &self->items[i];
}

static inline void free_boolarray(BoolArray self) { free(self.items); }

typedef struct objlist {
  /// Null for init.
  GcPtr *items;
  usize len;
  usize cap;
} ObjList;

static inline void free_objlist(ObjList self);

typedef void (*DestroyCallback)(void *);

#define NO_DESTORY_CALLBACK ((DestroyCallback) nullptr)

#define DEBUG_LOG

typedef struct obj_metadata {
  ObjList reflist;
  usize strong_count;
  // Function to be called when freeing this object.
  // Null for do nothing.
  // Must not call `gc_mark_dead` on child GC objects, as this is managed by its reflist.
  DestroyCallback destroy_callback;
} ObjMetadata;

static inline void free_metadata(ObjMetadata self) { free_objlist(self.reflist); }

struct gcptr {
  /// Nullable
  void *obj;
  ObjMetadata *metadata;
};

static inline void print_gcptr_addr(GcPtr p) { printf("gcptr(obj: %p, metadata: %p)", p.obj, p.metadata); }

static inline void println_gcptr_addr(GcPtr p) {
  print_gcptr_addr(p);
  printf("\n");
}

#define OBJLIST_INIT_CAP 32

static inline ObjList new_with_capacity_objlist(usize cap);

static inline ObjList new_objlist() { return (ObjList){0}; }

static inline ObjList new_with_capacity_objlist(usize cap) {
  if (cap == 0) {
    return new_objlist();
  }
  GcPtr *p = xalloc(GcPtr, cap);
  return (ObjList){.items = p, .len = 0, .cap = cap};
}

static inline void push_objlist(ObjList *self, GcPtr item) {
  if (self->items == nullptr) {
    DEBUG_ASSERT(self->cap == 0);
    DEBUG_ASSERT(self->len == 0);
    self->cap = OBJLIST_INIT_CAP;
    self->len = 1;
    self->items = xalloc(GcPtr, self->cap);
    self->items[0] = item;
    return;
  }
  self->len += 1;
  if (self->cap < self->len) {
    self->cap *= 2;
    self->items = xrealloc(self->items, GcPtr, self->cap);
  }
  self->items[self->len - 1] = item;
}

static inline void debug_check_range_objlist(ObjList *self, usize i) { DEBUG_ASSERT(i < self->len); }

static inline GcPtr *get_item_objlist(ObjList *self, usize i) {
  debug_check_range_objlist(self, i);
  return &self->items[i];
}

static inline void free_objlist(ObjList self) {
  if (self.items != nullptr)
    xfree(self.items);
}

/// Not thread safe.
typedef struct gcarena {
  ObjList objects;
} GcArena;

GcArena gc_new_arena() {
  return (GcArena){
      .objects = new_objlist(),
  };
}

static inline void destroy_object(GcPtr object) {
  if (object.metadata->destroy_callback != nullptr && object.obj != nullptr) {
    (object.metadata->destroy_callback)((void *)object.obj);
  }
  xfree(object.obj);
  free_metadata(*object.metadata);
}

void free_gcarena(GcArena self) {
  for (usize i = 0; i < self.objects.len; ++i) {
    GcPtr object = *get_item_objlist(&self.objects, i);
    destroy_object(object);
  }
  free_objlist(self.objects);
}

GcPtr gc_clone(GcPtr p) {
  p.metadata->strong_count += 1;
  return p;
}

static inline bool gcobject_alive(GcPtr object) { return object.metadata->strong_count != 0; }

static inline void gcarena_recursive_mark_alive(GcArena *self, GcPtr object, usize object_idx, BoolArray *markers) {
  if (gcobject_alive(object)) {
    *get_item_boolarray(markers, object_idx) = true;
    for (usize i = 0; i < object.metadata->reflist.len; ++i) {
      GcPtr object_ = *get_item_objlist(&object.metadata->reflist, i);
      gcarena_recursive_mark_alive(self, object_, i, markers);
    }
  }
}

static inline void gcarena_perform_destroys(GcArena *self, BoolArray *markers) {
  ObjList new_objects = new_objlist();
  for (usize i = 0; i < self->objects.len; ++i) {
    bool is_alive = *get_item_boolarray(markers, i);
    if (is_alive) {
      GcPtr object = *get_item_objlist(&self->objects, i);
      push_objlist(&new_objects, object);
    } else {
      GcPtr object = *get_item_objlist(&self->objects, i);
#ifdef DEBUG_LOG
      DBG_PRINTF("destroying object: ");
      println_gcptr_addr(object);
#endif
      destroy_object(object);
    }
  }
  self->objects = new_objects;
}

void gc_sweep(GcArena *self) {
#ifdef DEBUG_LOG
  DBG_PRINTF("Sweeping starts\n");
#endif
  BoolArray markers = new_boolarray(self->objects.len);
  for (usize i = 0; i < self->objects.len; ++i) {
    GcPtr object = *get_item_objlist(&self->objects, i);
    gcarena_recursive_mark_alive(self, object, i, &markers);
  }
  gcarena_perform_destroys(self, &markers);
  free_boolarray(markers);
}

/// `value` is the unique pointer to the value on heap.
GcPtr gc_new_object(GcArena *self, void *restrict value, ObjList reflist, DestroyCallback destroy_callback) {
  ObjMetadata metadata = (ObjMetadata){
      .reflist = reflist,
      .strong_count = 1,
      .destroy_callback = destroy_callback,
  };
  GcPtr obj = (GcPtr){
      .obj = value,
      .metadata = PUT_ON_HEAP(metadata),
  };
  push_objlist(&self->objects, obj);
  return obj;
}

void gc_mark_dead(GcPtr object) {
  DEBUG_ASSERT(object.metadata->strong_count != 0);
  object.metadata->strong_count -= 1;
}

#define GC_GET(TY, PTR) ((const TY *)((PTR).obj))

typedef struct node {
  GcPtr next;
} Node;

static inline GcPtr node_to_gcobject(GcArena *arena, Node node) {
  ObjList objlist = new_with_capacity_objlist(1);
  push_objlist(&objlist, node.next);
  return gc_new_object(arena, PUT_ON_HEAP(node), objlist, NO_DESTORY_CALLBACK);
}

i32 main() {
  GcArena arena = gc_new_arena();

  GcPtr node0 = node_to_gcobject(&arena, (Node){0});
  GcPtr node1 = node_to_gcobject(&arena, (Node){.next = node0});
  GcPtr node2 = node_to_gcobject(&arena, (Node){.next = node1});

  // Some not so safe code here to mutate values inside a GcPtr.
  PTR_CAST(Node *, node0.obj)->next = node2;
  node0.metadata->reflist.items[0] = node2;

  println_gcptr_addr(node0);
  println_gcptr_addr(node1);
  println_gcptr_addr(node2);

  gc_mark_dead(node0);
  gc_mark_dead(node1);
  gc_mark_dead(node2);

  gc_sweep(&arena);
  gc_sweep(&arena);

  free_gcarena(arena);
  return 0;
}
