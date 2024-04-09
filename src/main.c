#include "common.h"
#include "debug_utils.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gcptr GcPtr;

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

static inline void gcarena_sweep_refs(GcArena *self, GcPtr object) {
  if (!gcobject_alive(object)) {
    for (usize i = 0; i < object.metadata->reflist.len; ++i) {
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
      destroy_object(object);
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

typedef struct test_obj {
  GcPtr child_i32_0;
  GcPtr string;
} TestObj;

ObjList test_obj_reflist(TestObj *self) {
  DEBUG_ASSERT(self->child_i32_0.obj != nullptr);
  DEBUG_ASSERT(self->string.obj != nullptr);
  ObjList reflist = new_with_capacity_objlist(2);
  push_objlist(&reflist, self->child_i32_0);
  push_objlist(&reflist, self->string);
  return reflist;
}

const char **get_string(GcPtr test_obj) {
  const TestObj *test_obj_ = GC_GET(TestObj, test_obj);
  return GC_GET(char *, test_obj_->string);
}

void destroy_string(void *p) {
  char *s = *PTR_CAST(char **, p);
  DBG_PRINTF("%s\n", s);
  xfree(s);
}

i32 main() {
  GcArena arena = gc_new_arena();

  // The simplist GC object (No custom destroyer, no child objects).
  i32 number_ = 10;
  GcPtr number = gc_new_object(&arena, PUT_ON_HEAP(number_), new_objlist(), NO_DESTORY_CALLBACK);

  // A GC Object with a custome destroyer.
  const char s[] = "hello, world";
  char *string_ = xalloc(char, sizeof(s));
  memcpy(string_, s, sizeof(s));
  GcPtr string = gc_new_object(&arena, PUT_ON_HEAP(string_), new_objlist(), &destroy_string);

  // Create test_obj1 object.
  TestObj test_obj1_ = (TestObj){
      .child_i32_0 = gc_clone(number),
      .string = gc_clone(string),
  };
  GcPtr test_obj1 = gc_new_object(&arena, PUT_ON_HEAP(test_obj1_), test_obj_reflist(&test_obj1_), NO_DESTORY_CALLBACK);

  // Create test_obj2 object.
  TestObj test_obj2_ = (TestObj){
      .child_i32_0 = number,
      .string = string,
  };
  GcPtr test_obj2 = gc_new_object(&arena, PUT_ON_HEAP(test_obj2_), test_obj_reflist(&test_obj2_), NO_DESTORY_CALLBACK);

  // Print out addresses of GC pointers so later we can see which objects are destroyed.
  DBG_PRINTF("number    = ");
  println_gcptr_addr(number);
  DBG_PRINTF("string    = ");
  println_gcptr_addr(string);
  DBG_PRINTF("test_obj1 = ");
  println_gcptr_addr(test_obj1);
  DBG_PRINTF("test_obj2 = ");
  println_gcptr_addr(test_obj2);

  // Since we defined `DEBUG_LOG` ealier the `gc_sweep` functions would log which objects are destroyed.
  gc_sweep(&arena); // Expect: No objects are destroyed.
  gc_mark_dead(test_obj1);
  gc_sweep(&arena);                  // Expect: `test_obj1` is destroyed.
  DBG_PRINT(*get_string(test_obj2)); // `test_obj2` is still alive and using `string`, so `string` is not destroyed.
  gc_mark_dead(test_obj2);
  gc_sweep(&arena); // Expect: `test_obj2`, `number`, `string` are all destroyed.
  gc_sweep(&arena); // Expect: No Objects are destroyed.

  free_gcarena(arena);
  return 0;
}
