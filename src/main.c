
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
  GcPtr node1 = node_to_gcobject(&arena, (Node){.next = node0});

  // Some not so safe code here to mutate values inside a GcPtr.
  PTR_CAST(Node *, node0.obj)->next = node1;
  node0.metadata->reflist.items[0] = node1;

  printf("node0 = ");
  gc_println_ptr_addr(&node0);
  printf("node1 = ");
  gc_println_ptr_addr(&node1);

  gc_enters_scope(node0);
  gc_sweep(&arena);           // Expect: no object destroyed.
  gc_leaves_scope(node0);
  gc_sweep(&arena);           // Expect: both nodes destroyed.
  gc_sweep(&arena);           // Expect: no object destroyed.

  gc_free_arena(arena);
  return 0;
}

