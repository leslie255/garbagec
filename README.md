# GarbageC

**Tracing GC (a.k.a. Mark & Sweep GC) implemented in C**

It is designed for a possible future programming language project.

For now it's not hygienically packaged into a library.

## Demo

Note that this demo code requires `common.h` and `debug_utils.h`, as well as `#define DEBUG_LOG` so the `gc_sweep`
function logs what objects are destroyed.

```c

/// A TestObj has two child i32 values allocated in GC arena.
typedef struct test_obj {
  GcPtr child_i32_0;
  GcPtr child_i32_1;
} TestObj;

/// A GC object needs to have a reflist for all child objects it has.
/// `reflist`s takes the type of `ObjList`, a dynamic array of `GcPtr`s.
ObjList test_obj_reflist(TestObj *self) {
  /// `DEBUG_ASSERT` is a macro defined in `common.h`.
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
  // `PUT_ON_HEAP` is a macro in `common.h` for boxing a value onto the heap.
  GcPtr number0 = gc_new_object(&arena,
                                PUT_ON_HEAP(number0_),
                                new_objlist(),
                                NO_DESTORY_CALLBACK);

  // Create number1 object.
  i32 number1_ = 255;
  GcPtr number1 = gc_new_object(&arena,
                                PUT_ON_HEAP(number1_),
                                new_objlist(),
                                NO_DESTORY_CALLBACK);

  // Create test_obj1 object which contains one alias of `number0` and
  // `number1`.
  TestObj test_obj1_ = (TestObj){
    .child_i32_0 = gc_clone(number0),
    .child_i32_1 = gc_clone(number1),
  };
  GcPtr test_obj1 = gc_new_object(&arena,
                                  PUT_ON_HEAP(test_obj1_),
                                  test_obj_reflist(&test_obj1_),
                                  NO_DESTORY_CALLBACK);

  // Create `test_obj1` object which contains another alias (but the same copy
  // in memory!) of `number0` and `number1`.
  TestObj test_obj2_ = (TestObj){
    .child_i32_0 = number0, // Unlike for `test_obj1`, here we're moving the
                            // number objects into `test_obj2`.
    .child_i32_1 = number1,
  };
  GcPtr test_obj2 = gc_new_object(&arena,
                                  PUT_ON_HEAP(test_obj2_),
                                  test_obj_reflist(&test_obj2_),
                                  NO_DESTORY_CALLBACK);

  // Print out addresses of GC pointers so later we can see which objects are
  // destroyed.
  DBG_PRINTF("number0   = "); println_gcptr_addr(number0);
  DBG_PRINTF("number1   = "); println_gcptr_addr(number1);
  DBG_PRINTF("test_obj1 = "); println_gcptr_addr(test_obj1);
  DBG_PRINTF("test_obj2 = "); println_gcptr_addr(test_obj2);

  // Since we defined `DEBUG_LOG` ealier the `gc_sweep` functions would log
  // which objects are destroyed.
  gc_sweep(&arena); // Expect: No objects are destroyed.
  gc_mark_dead(test_obj1);
  gc_sweep(&arena); // Expect: `test_obj1` is destroyed.
  gc_mark_dead(test_obj2);
  gc_sweep(&arena); // Expect: `test_obj2`, `number0`, `number1` are destroyed.
  gc_sweep(&arena); // Expect: No Objects are destroyed.

  free_gcarena(arena);
  return 0;
}
```

Running the above code produces:

```
$ make all MODE=release
clang -Wall -Wconversion --std=gnu2x -g -O0 -DDEBUG -c src/main.c -o bin/main.o
clang -Wall -Wconversion --std=gnu2x -g -O0 -DDEBUG bin/*.o -o bin/garbagec
$ ./bin/garbagec                                   
[main@src/main.c:210] number0   = gcptr(obj: 0x13d605fe0, metadata: 0x13d605f40)
[main@src/main.c:211] number1   = gcptr(obj: 0x13d6061a0, metadata: 0x13d606100)
[main@src/main.c:212] test_obj1 = gcptr(obj: 0x13d606080, metadata: 0x13d605dd0)
[main@src/main.c:213] test_obj2 = gcptr(obj: 0x13d605e00, metadata: 0x13d605e40)
[gc_sweep@src/main.c:139] Sweeping starts
[gc_sweep@src/main.c:139] Sweeping starts
[gcarena_perform_destroys@src/main.c:124] destroying object: gcptr(obj: 0x13d606080, metadata: 0x13d605dd0)
[gc_sweep@src/main.c:139] Sweeping starts
[gcarena_perform_destroys@src/main.c:124] destroying object: gcptr(obj: 0x13d605fe0, metadata: 0x13d605f40)
[gcarena_perform_destroys@src/main.c:124] destroying object: gcptr(obj: 0x13d6061a0, metadata: 0x13d606100)
[gcarena_perform_destroys@src/main.c:124] destroying object: gcptr(obj: 0x13d605e00, metadata: 0x13d605e40)
[gc_sweep@src/main.c:139] Sweeping starts
```

## LICENSE

This project is licensed under GPLv3.
