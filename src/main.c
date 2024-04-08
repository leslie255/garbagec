#include "common.h"
#include "debug_utils.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct gcptr GcPtr;

typedef struct obj_list {
  /// Must be non null
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

static inline ObjList new_objlist() { return new_with_capacity_objlist(OBJLIST_INIT_CAP); }

/// Panics if cap is zero.
static inline ObjList new_with_capacity_objlist(usize cap) {
  ASSERT(cap != 0);
  GcPtr *p = xalloc(GcPtr, cap);
  return (ObjList){.items = p, .len = 0, .cap = cap};
}

static inline void push_objlist(ObjList *self, GcPtr item) {
  DEBUG_ASSERT(self->items != nullptr);
  self->len += 1;
  if (self->cap < self->len) {
    self->cap *= 2;
    self->items = xrealloc(self->items, GcPtr, self->cap);
  }
  self->items[self->len - 1] = item;
}

static inline void debug_check_range_objlist(ObjList *self, usize i) {
  DEBUG_ASSERT(self->items != nullptr);
  DEBUG_ASSERT(i < self->len);
}

static inline GcPtr *get_item_objlist(ObjList *self, usize i) {
  debug_check_range_objlist(self, i);
  return &self->items[i];
}

static inline void free_objlist(ObjList self) { xfree(self.items); }

/// Not thread safe.
typedef struct gcarena {
  ObjList objects;
} GcArena;

GcArena gc_new_arena() {
  return (GcArena){
      .objects = new_objlist(),
  };
}

void free_gcarena(GcArena self) { free_objlist(self.objects); }

GcPtr gc_clone(GcPtr p) {
  p.metadata->strong_count += 1;
  return p;
}

static inline bool gcobject_alive(GcPtr object) { return object.metadata->strong_count != 0; }

static inline void gcarena_sweep_refs(GcArena *self, GcPtr object) {
  /// TODO: This could be optimized by not sweeping objects we've already sweeped before on this round.
  if (!gcobject_alive(object)) {
    for (usize i = 0; i < object.metadata->reflist.len; i++) {
      GcPtr object_ = *get_item_objlist(&object.metadata->reflist, i);
      object_.metadata->strong_count -= 1;
      gcarena_sweep_refs(self, object_);
    }
  }
}

static inline void gcarena_perform_destroys(GcArena *self) {
  ObjList new_objects = new_objlist();
  for (usize i = 0; i < self->objects.len; ++i) {
    GcPtr object = *get_item_objlist(&self->objects, i);
    bool is_alive = object.metadata->strong_count != 0;
    if (is_alive) {
      push_objlist(&new_objects, object);
    } else {
      GcPtr object = *get_item_objlist(&self->objects, i);
#ifdef DEBUG_LOG
      DBG_PRINTF("destroying object: ");
      println_gcptr_addr(object);
#endif
      if (object.metadata->destroy_callback != nullptr && object.obj != nullptr) {
        (object.metadata->destroy_callback)((void *)object.obj);
      }
      xfree(object.obj);
      free_metadata(*object.metadata);
    }
  }
  self->objects = new_objects;
}

void gc_sweep(GcArena *self) {
#ifdef DEBUG_LOG
  DBG_PRINTF("Sweeping starts\n");
#endif
  for (usize i = 0; i < self->objects.len; ++i) {
    gcarena_sweep_refs(self, *get_item_objlist(&self->objects, i));
  }
  gcarena_perform_destroys(self);
}

/// `value` is the unique pointer to the value on heap.
GcPtr gc_new_object(GcArena *self, void *restrict value, ObjList reflist, DestroyCallback free_callback) {
  DEBUG_ASSERT(value != nullptr);
  ObjMetadata metadata = (ObjMetadata){
      .reflist = reflist,
      .strong_count = 1,
      .destroy_callback = free_callback,
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

#define GC_DEREF(TY, PTR) (*(TY *)(PTR.obj))

typedef struct test_obj {
  GcPtr child_i32_0;
  GcPtr child_i32_1;
} TestObj;

ObjList test_obj_reflist(TestObj *self) {
  DEBUG_ASSERT(self->child_i32_0.obj != nullptr);
  DEBUG_ASSERT(self->child_i32_1.obj != nullptr);
  ObjList reflist = new_with_capacity_objlist(2);
  push_objlist(&reflist, self->child_i32_0);
  push_objlist(&reflist, self->child_i32_1);
  return reflist;
}

i32 main() {
  GcArena arena = gc_new_arena();

  // Create number0 object.
  i32 number0_ = 10;
  GcPtr number0 = gc_new_object(&arena, PUT_ON_HEAP(number0_), new_objlist(), NO_DESTORY_CALLBACK);

  // Create number1 object.
  i32 number1_ = 255;
  GcPtr number1 = gc_new_object(&arena, PUT_ON_HEAP(number1_), new_objlist(), NO_DESTORY_CALLBACK);

  // Create test_obj1 object.
  TestObj test_obj1_ = (TestObj){
      .child_i32_0 = gc_clone(number0),
      .child_i32_1 = gc_clone(number1),
  };
  GcPtr test_obj1 = gc_new_object(&arena, PUT_ON_HEAP(test_obj1_), test_obj_reflist(&test_obj1_), NO_DESTORY_CALLBACK);

  // Create test_obj2 object.
  TestObj test_obj2_ = (TestObj){
      .child_i32_0 = number0,
      .child_i32_1 = number1,
  };
  GcPtr test_obj2 = gc_new_object(&arena, PUT_ON_HEAP(test_obj2_), test_obj_reflist(&test_obj2_), NO_DESTORY_CALLBACK);

  // Print out addresses of GC pointers so later we can see which objects are destroyed.
  DBG_PRINTF("number0   = "); println_gcptr_addr(number0);
  DBG_PRINTF("number1   = "); println_gcptr_addr(number1);
  DBG_PRINTF("test_obj1 = "); println_gcptr_addr(test_obj1);
  DBG_PRINTF("test_obj2 = "); println_gcptr_addr(test_obj2);

  // Since we defined `DEBUG_LOG` ealier the `gc_sweep` functions would log which objects are destroyed.
  gc_sweep(&arena); // Expect: No objects are destroyed.
  gc_mark_dead(test_obj1);
  gc_sweep(&arena); // Expect: `test_obj` is destroyed.
  gc_mark_dead(test_obj2);
  gc_sweep(&arena); // Expect: `test_obj2`, `number0`, `number1` are destroyed.
  gc_sweep(&arena); // Expect: No Objects are destroyed.

  free_gcarena(arena);
  return 0;
}
