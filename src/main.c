#include "common.h"
#include "debug_utils.h"
#include "gc.h"

typedef struct node {
  GcPtr next;
} Node;

static inline GcPtr node_to_gcobject(GcArena *arena, Node node) {
  GcObjlist objlist = gc_new_objlist_with_capacity(1);
  gc_push_objlist(&objlist, node.next);
  return gc_new_object(arena, PUT_ON_HEAP(node), objlist, NO_DESTORY_CALLBACK);
}

i32 main() {
  GcArena arena = gc_new_arena();

  GcPtr node0 = node_to_gcobject(&arena, (Node){0});
  GcPtr node1 = node_to_gcobject(&arena, (Node){.next = gc_clone(node0)});
  GcPtr node2 = node_to_gcobject(&arena, (Node){.next = gc_clone(node1)});

  // Some not so safe code here to mutate values inside a GcPtr.
  PTR_CAST(Node *, node0.obj)->next = gc_clone(node2);
  node0.metadata->reflist.items[0] = node2;

  DBG_PRINTF("node0 = ");
  gc_println_ptr(&node0);
  DBG_PRINTF("node1 = ");
  gc_println_ptr(&node1);
  DBG_PRINTF("node2 = ");
  gc_println_ptr(&node2);

  gc_sweep(&arena);

  gc_mark_dead(node0);
  gc_mark_dead(node1);
  gc_mark_dead(node2);

  gc_sweep(&arena);
  // gc_sweep(&arena);

  // gc_free_arena(arena);
  return 0;
}
