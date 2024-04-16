# GarbageC

**Tracing GC (aka. Mark & Sweep GC) implemented in C**

It is designed for a runtime of a possible future programming language project, not use within C projects.

## Demo 1: Without circle referencing

```c
#include "common.h"
#include "debug_utils.h"
#include "gc.h"

/// A TestObj has two child i32 values allocated in GC arena.
typedef struct test_obj {
  GcPtr child_i32_0;
  GcPtr child_i32_1;
} TestObj;

/// A GC object needs to have a reflist for all child objects it has.
/// `reflist`s takes the type of `GcObjlist`, a dynamic array of `GcPtr`s.
GcObjlist test_obj_reflist(TestObj *self) {
  GcObjlist reflist = gc_new_objlist_with_capacity(2);
  gc_push_objlist(&reflist, self->child_i32_0);
  gc_push_objlist(&reflist, self->child_i32_1);
  return reflist;
}

i32 main() {
  GcArena arena = gc_new_arena();

  // Create number0 object.
  i32 number0_ = 10;
  // `PUT_ON_HEAP` is a macro in `common.h` for boxing a value onto the heap.
  GcPtr number0 = gc_new_object(&arena,
                                PUT_ON_HEAP(number0_),
                                gc_new_objlist(),
                                NO_DESTORY_CALLBACK);

  // Create number1 object.
  i32 number1_ = 255;
  GcPtr number1 = gc_new_object(&arena,
                                PUT_ON_HEAP(number1_),
                                gc_new_objlist(),
                                NO_DESTORY_CALLBACK);

  // Create `test_obj0` object which contains one alias of `number0` and
  // `number1`.
  TestObj test_obj0_ = (TestObj){
    .child_i32_0 = number0,
    .child_i32_1 = number1,
  };
  GcPtr test_obj0 = gc_new_object(&arena,
                                  PUT_ON_HEAP(test_obj0_),
                                  test_obj_reflist(&test_obj0_),
                                  NO_DESTORY_CALLBACK);
  gc_enters_scope(test_obj0);

  // Create `test_obj1` object which contains another alias of `number0` and
  // `number1`.
  TestObj test_obj1_ = (TestObj){
    .child_i32_0 = number0,
    .child_i32_1 = number1,
  };
  GcPtr test_obj1 = gc_new_object(&arena,
                                  PUT_ON_HEAP(test_obj1_),
                                  test_obj_reflist(&test_obj1_),
                                  NO_DESTORY_CALLBACK);
  gc_enters_scope(test_obj1);

  // Print out addresses of GC pointers so later we can see which objects are
  // destroyed.
  DBG_PRINTF("number0   = "); gc_println_ptr_addr(&number0);
  DBG_PRINTF("number1   = "); gc_println_ptr_addr(&number1);
  DBG_PRINTF("test_obj0 = "); gc_println_ptr_addr(&test_obj0);
  DBG_PRINTF("test_obj1 = "); gc_println_ptr_addr(&test_obj1);

  gc_sweep(&arena); // Expect: No objects are destroyed.
  gc_leaves_scope(test_obj0);
  gc_sweep(&arena); // Expect: `test_obj0` is destroyed.
  gc_leaves_scope(test_obj1);
  gc_sweep(&arena); // Expect: `test_obj1`, `number0`, `number1` are destroyed.
  gc_sweep(&arena); // Expect: No Objects are destroyed.

  gc_free_arena(arena);
  return 0;
}
```

Running the above code produces:

```
$ mkdir bin/
$ make all MODE=release
clang -Wall -Wconversion --std=gnu2x -O2 -c src/main.c -o bin/main.o
clang -Wall -Wconversion --std=gnu2x -O2 bin/*.o -o bin/garbagec
$ ./bin/garbagec                                   
[main@src/main.c:67] number0   = gcptr(obj: 0x152704080, metadata: 0x152704090)
[main@src/main.c:68] number1   = gcptr(obj: 0x1527042c0, metadata: 0x1527042d0)
[main@src/main.c:69] test_obj1 = gcptr(obj: 0x152704300, metadata: 0x152704340)
[main@src/main.c:70] test_obj2 = gcptr(obj: 0x152704370, metadata: 0x1527043b0)
[gc_sweep@src/gc.c:113] Sweeping starts
[gc_sweep@src/gc.c:113] Sweeping starts
[do_destroys@src/gc.c:102] destroying object: gcptr(obj: 0x152704300, metadata: 0x152704340)
[gc_sweep@src/gc.c:113] Sweeping starts
[do_destroys@src/gc.c:102] destroying object: gcptr(obj: 0x152704080, metadata: 0x152704090)
[do_destroys@src/gc.c:102] destroying object: gcptr(obj: 0x1527042c0, metadata: 0x1527042d0)
[do_destroys@src/gc.c:102] destroying object: gcptr(obj: 0x152704370, metadata: 0x1527043b0)
[gc_sweep@src/gc.c:113] Sweeping starts
```

### Demo 2: With circle referencing

```c
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
```

Result:

```
$ mkdir bin/
$ make all MODE=release
clang -Wall -Wconversion --std=gnu2x -O2 -c src/main.c -o bin/main.o
clang -Wall -Wconversion --std=gnu2x -O2 -c src/gc.c -o bin/gc.o
clang -Wall -Wconversion --std=gnu2x -O2 bin/main.o bin/gc.o -o bin/garbagec
$ ./bin/garbagec
node0 = gcptr(obj: 0x158e061e0, metadata: 0x158e05f80)
node1 = gcptr(obj: 0x158e06140, metadata: 0x158e06150)
[gc_sweep@src/gc.c:113] Sweeping starts
[gc_sweep@src/gc.c:113] Sweeping starts
[do_destroys@src/gc.c:102] destroying object: gcptr(obj: 0x158e061e0, metadata: 0x158e05f80)
[do_destroys@src/gc.c:102] destroying object: gcptr(obj: 0x158e06140, metadata: 0x158e06150)
[gc_sweep@src/gc.c:113] Sweeping starts
```

## LICENSE

This project is licensed under GPLv3.
