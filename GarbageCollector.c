#include <stdio.h>
#include <stdlib.h>

/* The number of objects in our system. */
#define STACK_MAX_SIZE 256

/* The INITIAL_GC_THRESHOLD will be the number of objects at 
   which you kick off the first GC. A smaller number is more conservative with memory,
   a larger number spends less time on garbage collection. */
#define INITIAL_GC_THRESHOLD 16

/* DEFENITION OF TYPES.
   Our language has two types, INT_TYPE and PAIR_TYPE.
   PAIR can contain more pairs or ints. */

/* Objects type definitions. */
typedef enum {
  INT_TYPE,
  PAIR_TYPE
} ObjectType;

typedef struct sObject {
  /* What type of object is this? */
  ObjectType objectType;

  /* Marked for collection. 
     Note that storing this in the object is a bad idea, 
     because GC will cause copy-on-write to go nuts 
     in the event you are using fork(). */
  unsigned char marked;

  /* This field allows us to make the Object 
     a node in the VM's linked list of Objects. */
  struct sObject* next;

  /* A union to hold the data for the int or pair. If your C is rusty, a
     union is a struct where the fields overlap in memory. Since a given
     object can only be an int or a pair, there’s no reason to have memory in
     a single object for all three fields at the same time. A union does
     that. Groovy. */
  union {
    /* INT_TYPE */
    int value;

    /* PAIR_TYPE */
    struct {
      struct sObject* head;
      struct sObject* tail;
    };
  };
} Object;

/* OUR MINIMAL VIRTUAL MACHINE (VM). */

typedef struct {
  /* The stack. */
  Object* stack[STACK_MAX_SIZE];

  /* The current size of the stack. */
  int stackSize;
  /* The total number of currently allocated objects. */
  int numObjects;
  /* The number of objects required to trigger a GC. */
  int maxObjects;
  /* The first object in the list of all objects. */

  Object* firstObject;
} VM;

/* Function that creates and initializes a VM. */
VM* newVM() {
  VM* vm = (VM*)malloc(sizeof(VM));
  vm->stackSize = 0;
  vm->numObjects = 0;
  vm->maxObjects = INITIAL_GC_THRESHOLD;
  vm->firstObject = NULL;
  return vm;
}

/* VM MANIPULATION.
   We need to be able to manipulate our VM stack. */
/* Function for adding an object from the stack. */
void push(VM* vm, Object* value) {
  /* Message about error */
  if (vm->stackSize > STACK_MAX_SIZE) {
    perror("Stack overflow!\n");
    exit(EXIT_FAILURE);
  }
  
  /* Increase the size of the stack when adding the object */
  vm->stack[vm->stackSize++] = value;
}

/* Function for removing an object from the stack. */
Object* pop(VM* vm) {
  if (!(vm->stackSize > 0)) {
    perror("Stack underflow!\n");
    exit(EXIT_FAILURE);
  }

  /* Reduce stack size when deleting an object. */
  return vm->stack[--(vm->stackSize)];
}

/* Method declaration, description below */
void gc(VM* vm);

/*  We need to be able to actually create objects. */
Object* newObject(VM* vm, ObjectType objectType) {
  /* If the number of objects in the stack is equal to the max objects
     threshold, run the garbage collector. */
  if (vm->numObjects == vm->maxObjects) gc(vm);

  /* Allocate the new object. */
  Object* object = (Object*)malloc(sizeof(Object));
  /* Set it's type. */
  object->objectType = objectType;
  /* Init marked to zero. */
  object->marked = 0;

  /* Insert it to the list of allocated objects. */
  object->next = vm->firstObject;
  vm->firstObject = object;

  /* Increment the object count */
  vm->numObjects++;

  /* Return the object back to our caller. */
  return object;
}

/* Using method newObject, we can write functions
   to push each kind of object onto the VM's stack. */
/* Push to stack INT_TYPE object. */
void pushInt(VM* vm, int intValue) {
  Object* object = newObject(vm, INT_TYPE);
  object->value = intValue;
  push(vm, object);
}

/* Push to stack PAIR_TYPE object. */
Object* pushPair(VM* vm) {
  Object* object = newObject(vm, PAIR_TYPE);
  object->tail = pop(vm);
  object->head = pop(vm);

  push(vm, object);
  return object;
}

/* MARKING.
   We need to walk all of the reachable objects 
   and set their mark bit. The first thing we need 
   then is to add a mark bit to Object. */

/* We add to struct Object marked definition and 
   when we create a new object, we’ll modify newObject() 
   to initialize marked to zero. */

void mark(Object* object) {
  /* Return if we've already marked this one. 
     This prevents loop and stack overflow. */
  if (object->marked) return;

  /* Mark the object as reachable. */
  object->marked = 1;

  /* If the object is a pair, its two fields are reachable too. */
  if (object->objectType == PAIR_TYPE) {
    mark(object->head);
    mark(object->tail);
  }
}

/* To mark all of the reachable objects, we start with the variables that are
   in memory, so that means walking the stack. Using mark() this will
   mark every object in memory that is reachable. */
void markAll(VM* vm) {
  for (int i = 0; i < vm->stackSize; i++) {
    mark(vm->stack[i]);
  }
}

/* SWEEPING.
   The next phase is to sweep through all of the objects we’ve allocated 
   and free any of them that aren’t marked. */

/* But all of the unmarked objects are, by definition, unreachable! 
   We can’t get to them! */

/* The simplest way to do this is to just maintain 
   a linked list of every object we’ve ever allocated.
   We’ll extend Object itself to be a node in that list. 
   And the VM will keep track of the head of that list. */

void sweep(VM* vm) {
  /* Pointer to a pointer to first object in VM's list. */
  Object** object = &vm->firstObject;
  /* Iterate the list and check marking of elements. */
  while (*object) {
    /* This object wasn't reached, so remove it from the list and free it. */
    if (!(*object)->marked) {
      Object* unreached = *object;

      *object = unreached->next;
      free(unreached);
    }
    /* This object was reached, so unmark it (for the next GC) 
       and move on to the next. */
    else {
      (*object)->marked = 0;
      object = &(*object)->next;
    }
  }
}

/* GARBAGE COLLECTOR (GC). */
void gc(VM* vm) {
  int numObjects = vm->numObjects;

  /* Mark all of the reachable objects. */
  markAll(vm);
  /* Remove unreachable objects from the list and free it. */
  sweep(vm);

  /* Change the threshold for the next collection to 2 times the new
     number of objects. */
  vm->maxObjects = vm->numObjects * 2;
}

/* TESTS. */
int main() {
  /* Create a new VM. */
  VM* vm = newVM();
  //vm->maxObjects = 4;

  printf("Adding integer 0 to the stack.\n");
  pushInt(vm, 0);

  printf("Adding integer 1 to the stack.\n");
  pushInt(vm, 1);

  printf("Adding integer 2 to the stack.\n");
  pushInt(vm, 2);

  printf("Adding a pair to the stack (consuming three ints already there).\n");
  pushPair(vm);

  printf("Adding a pair to the stack (consuming three ints and pair already there).\n");
  pushPair(vm);

  printf("There are now %d objects in stack and %d objects have been allocated.\n", vm->stackSize, vm->numObjects);

  /* Remove it from the stack, simulating the variable no longer being referenced. */
  printf("Popping last pair from the stack.\n");
  Object* o = pop(vm);

  printf("There are now %d objects in stack and %d objects have been allocated.\n", vm->stackSize, vm->numObjects);

  printf("Manual invoking GC (should free all)");
  gc(vm);

  printf("There are now %d objects in stack and %d objects have been allocated.\n", vm->stackSize, vm->numObjects);
  
  /* Done with the VM! */
  free(vm);
  return 0;
}
