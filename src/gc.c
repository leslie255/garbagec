#define DEBUG_LOG

#include "gc.h"
#include "common.h"
#include "debug_utils.h"

void gc_free_metadata(GcMetadata self) { gc_free_objlist(self.reflist); }

#define OBJLIST_INIT_CAP 32

GcObjlist gc_new_objlist() { return (GcObjlist){0}; }

GcObjlist gc_new_objlist_with_capacity(usize cap) {
  if (cap == 0) {
    return gc_new_objlist();
  }
  GcPtr *p = xalloc(GcPtr, cap);
  return (GcObjlist){.items = p, .len = 0, .cap = cap};
}

void gc_push_objlist(GcObjlist *self, GcPtr item) {
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

void debug_check_range_objlist(GcObjlist *self, usize i) { DEBUG_ASSERT(i < self->len); }

GcPtr *gc_get_item_objlist(GcObjlist *self, usize i) {
  debug_check_range_objlist(self, i);
  return &self->items[i];
}

void gc_free_objlist(GcObjlist self) { xfree(self.items); }

GcArena gc_new_arena() {
  return (GcArena){
      .objects = gc_new_objlist(),
  };
}

static inline void destroy_object(GcPtr object) {
  if (object.metadata->destroy_callback != nullptr && object.obj != nullptr) {
    (object.metadata->destroy_callback)((void *)object.obj);
  }
  xfree(object.obj);
  gc_free_metadata(*object.metadata);
}

void gc_free_arena(GcArena self) {
  for (usize i = 0; i < self.objects.len; ++i) {
    GcPtr object = *gc_get_item_objlist(&self.objects, i);
    destroy_object(object);
  }
  gc_free_objlist(self.objects);
}

GcPtr gc_clone(GcPtr p) {
  p.metadata->strong_count += 1;
  return p;
}

static inline bool object_is_alive(GcPtr object) { return object.metadata->strong_count != 0; }

static inline bool object_seen_this_round(GcArena *const self, GcPtr object) {
  return object.metadata->last_seen_alive == self->sweep_count;
}

static inline void object_mark_alive(GcArena *self, GcPtr object) {
  object.metadata->last_seen_alive = self->sweep_count;
}

static inline void gcarena_recursive_mark_alive(GcArena *self, GcPtr object) {
  print_stacktrace();
  DBG_PRINTF("looking at: ");
  gc_println_ptr(&object);
  if (object_seen_this_round(self, object)) {
    DBG_PRINTF("seen before, skipping\n");
    return;
  }
  DBG_PRINTF("children:");
  gc_println_objlist(&object.metadata->reflist);
  object_mark_alive(self, object);
  if (object_is_alive(object)) {
    for (usize i = 0; i < object.metadata->reflist.len; ++i) {
      GcPtr child = *gc_get_item_objlist(&object.metadata->reflist, i);
      gcarena_recursive_mark_alive(self, child);
    }
  }
}

static inline void do_destroys(GcArena *self) {
  GcObjlist new_objects = gc_new_objlist();
  for (usize i = 0; i < self->objects.len; ++i) {
    GcPtr object = *gc_get_item_objlist(&self->objects, i);
    bool is_alive = object.metadata->last_seen_alive == self->sweep_count;
    if (is_alive) {
      gc_push_objlist(&new_objects, object);
    } else {
      GcPtr object = *gc_get_item_objlist(&self->objects, i);
#ifdef DEBUG_LOG
      DBG_PRINTF("destroying object: ");
      gc_println_ptr_addr(&object);
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
  self->sweep_count += 1;
  DBG_PRINT(self->sweep_count);
  DBG_PRINTF("Before:\n");
  for (usize i = 0; i < self->objects.len; ++i) {
    gc_println_ptr(gc_get_item_objlist(&self->objects, i));
  }
  for (usize i = 0; i < self->objects.len; ++i) {
    GcPtr object = *gc_get_item_objlist(&self->objects, i);
    gcarena_recursive_mark_alive(self, object);
  }
  DBG_PRINTF("After:\n");
  for (usize i = 0; i < self->objects.len; ++i) {
    gc_println_ptr(gc_get_item_objlist(&self->objects, i));
  }
  do_destroys(self);
}

/// `value` is the unique pointer to the value on heap.
GcPtr gc_new_object(GcArena *self, void *restrict value, GcObjlist reflist, DestroyCallback destroy_callback) {
  GcMetadata metadata = (GcMetadata){
      .reflist = reflist,
      .strong_count = 1,
      .destroy_callback = destroy_callback,
      .last_seen_alive = self->sweep_count,
  };
  GcPtr obj = (GcPtr){
      .obj = value,
      .metadata = PUT_ON_HEAP(metadata),
  };
  gc_push_objlist(&self->objects, obj);
  return obj;
}

void gc_mark_dead(GcPtr object) {
  DEBUG_ASSERT(object.metadata->strong_count != 0);
  object.metadata->strong_count -= 1;
}
