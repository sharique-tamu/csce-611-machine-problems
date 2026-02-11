/*
    File: kernel.C

    Author: R. Bettati
            Department of Computer Science
            Texas A&M University
    Date  : 2024/08/20


    This file has the main entry point to the operating system.

*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define MB *(0x1 << 20)
#define KB *(0x1 << 10)
/* Makes things easy to read */

#define KERNEL_POOL_START_FRAME ((2 MB) / (4 KB))
#define KERNEL_POOL_SIZE ((2 MB) / (4 KB))
#define PROCESS_POOL_START_FRAME ((4 MB) / (4 KB))
#define PROCESS_POOL_SIZE ((28 MB) / (4 KB))
/* Definition of the kernel and process memory pools */

#define MEM_HOLE_START_FRAME ((15 MB) / (4 KB))
#define MEM_HOLE_SIZE ((1 MB) / (4 KB))
/* We have a 1 MB hole in physical memory starting at address 15 MB */

#define TEST_START_ADDR_PROC (4 MB)
#define TEST_START_ADDR_KERNEL (2 MB)
/* Used in the memory test below to generate sequences of memory references. */
/* One is for a sequence of memory references in the kernel space, and the   */
/* other for memory references in the process space. */

#define N_TEST_ALLOCATIONS 32
/* Number of recursive allocations that we use to test.  */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "console.H"
#include "machine.H" /* LOW-LEVEL STUFF   */

#include "assert.H"
#include "cont_frame_pool.H" /* The physical memory manager */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

void test_memory(ContFramePool *_pool, unsigned int _allocs_to_go);
void test_max_space(ContFramePool *_pool, unsigned int max_frames);
void test_multiple_allocations_and_contiguous_mem(ContFramePool *_pool,
                                                  unsigned int rem_frames,
                                                  unsigned long &start);
void test_needed_info_frames(ContFramePool *_pool);
/*--------------------------------------------------------------------------*/
/* MAIN ENTRY INTO THE OS */
/*--------------------------------------------------------------------------*/

int main() {

  Console::init();
  Console::redirect_output(true); // comment if you want to stop redirecting
                                  // qemu window output to stdout

  /* -- INITIALIZE FRAME POOLS -- */

  /* ---- KERNEL POOL -- */

  ContFramePool kernel_mem_pool(KERNEL_POOL_START_FRAME, KERNEL_POOL_SIZE, 0);

  /* ---- PROCESS POOL -- */

  /*  // In later machine problems, we will be using two pools. You may want to
     comment this out and test
      // the management of two pools.

      unsigned long n_info_frames =
     ContFramePool::needed_info_frames(PROCESS_POOL_SIZE);

      unsigned long process_mem_pool_info_frame =
     kernel_mem_pool.get_frames(n_info_frames);

      ContFramePool process_mem_pool(PROCESS_POOL_START_FRAME,
                                     PROCESS_POOL_SIZE,
                                     process_mem_pool_info_frame);

      process_mem_pool.mark_inaccessible(MEM_HOLE_START_FRAME, MEM_HOLE_SIZE);
  */

  /* -- MOST OF WHAT WE NEED IS SETUP. THE KERNEL CAN START. */

  Console::puts("Hello World!\n");

  /* -- TEST MEMORY ALLOCATOR */

  test_memory(&kernel_mem_pool, N_TEST_ALLOCATIONS);

  /* ---- Add code here to test the frame pool implementation. */
  /* Running it after test_memory also ensures that frames are being
  freed, that is if get_frames actually allocates. */
  test_max_space(&kernel_mem_pool, KERNEL_POOL_SIZE);
  unsigned long i = 0;
  test_multiple_allocations_and_contiguous_mem(&kernel_mem_pool, 511, i);
  test_needed_info_frames(&kernel_mem_pool);
  /* -- NOW LOOP FOREVER */
  Console::puts("Testing is DONE. We will do nothing forever\n");
  Console::puts("Feel free to turn off the machine now.\n");

  for (;;)
    ;

  /* -- WE DO THE FOLLOWING TO KEEP THE COMPILER HAPPY. */
  return 1;
}

void test_memory(ContFramePool *_pool, unsigned int _allocs_to_go) {
  Console::puts("alloc_to_go = ");
  Console::puti(_allocs_to_go);
  Console::puts("\n");
  if (_allocs_to_go > 0) {
    // We have not reached the end yet.
    int n_frames =
        _allocs_to_go % 4 + 1; // number of frames you want to allocate
    unsigned long frame =
        _pool->get_frames(n_frames); // we allocate the frames from the pool
    int *value_array =
        (int *)(frame * (4 KB)); // we pick a unique number that we want to
                                 // write into the memory we just allocated
    for (int i = 0; i < (1 KB) * n_frames;
         i++) { // we write this value int the memory locations
      value_array[i] = _allocs_to_go;
    }
    test_memory(_pool,
                _allocs_to_go -
                    1); // recursively allocate and uniquely mark more memory
    for (int i = 0; i < (1 KB) * n_frames;
         i++) { // We check the values written into the memory before we
                // recursed
      if (value_array[i] !=
          _allocs_to_go) { // If the value stored in the memory locations is not
                           // the same that we wrote a few lines above then
                           // somebody overwrote the memory.
        Console::puts("MEMORY TEST FAILED. ERROR IN FRAME POOL\n");
        Console::puts("i =");
        Console::puti(i);
        Console::puts("   v = ");
        Console::puti(value_array[i]);
        Console::puts("   n =");
        Console::puti(_allocs_to_go);
        Console::puts("\n");
        for (;;)
          ; // We throw a fit.
      }
    }
    ContFramePool::release_frames(
        frame); // We free the memory that we allocated above.
  }
}

void test_max_space(ContFramePool *_pool, unsigned int max_frames) {
  // Leaving out 1st frame that is used for storing the bitmap.
  unsigned long n_frames = max_frames - 1;
  unsigned long frame =
      _pool->get_frames(n_frames); // we allocate the n_frames from the pool

  Console::puts("All frames allocated.\n");
  int *value_array =
      (int *)(frame * (4 KB)); // we pick a unique number that we want to
                               // write into the memory we just allocated
  for (int i = 0; i < (1 KB) * n_frames;
       i++) { // we write this value int the memory locations
    value_array[i] = i;
  }
  for (int i = 0; i < (1 KB) * n_frames;
       i++) { // We check the values written into the memory before we
              // recursed
    if (value_array[i] != i) { // If the value stored in the memory locations
                               // is not the same that we wrote a few lines
                               // above then somebody overwrote the memory.
      Console::puts("MEMORY TEST FAILED. ERROR IN FRAME POOL\n");
      Console::puts("i =");
      Console::puti(i);
      Console::puts("   v = ");
      Console::puti(value_array[i]);
      Console::puts("   n =");
      Console::puti(i);
      Console::puts("\n");
      for (;;)
        ; // We throw a fit.
    }
  }
  ContFramePool::release_frames(
      frame); // We free the memory that we allocated above.
  Console::puts("All frames freed.\n");
}

/* Should probably split this into two functions
 I now realise this function does the same as test_memory just it lets you
define no. of frames instead of no. of allocations. */
void test_multiple_allocations_and_contiguous_mem(ContFramePool *_pool,
                                                  unsigned int rem_frames,
                                                  unsigned long &start) {
  Console::puts("alloc_to_go = ");
  Console::puti(rem_frames);
  Console::puts("\n");
  if (rem_frames == 0)
    return;
  unsigned int n_frames;
  unsigned int frame;
  unsigned long cur_start = start;
  if (rem_frames < 10) {
    frame = _pool->get_frames(rem_frames);
    n_frames = rem_frames;
  } else {
    frame = _pool->get_frames(10);
    n_frames = 10;
  }

  unsigned int *value_array = (unsigned int *)(frame * (4 KB));
  for (int j = 0; j < (1 KB) * n_frames; j++) {
    value_array[j] = start++;
  }
  test_multiple_allocations_and_contiguous_mem(_pool, rem_frames - n_frames,
                                               start);
  for (int i = 0; i < (1 KB) * n_frames;
       i++) { // We check the values written into the memory before we
              // recursed
    if (value_array[i] !=
        cur_start++) { // If the value stored in the memory locations is not
                       // the same that we wrote a few lines above then
                       // somebody overwrote the memory.
      Console::puts("MEMORY TEST FAILED. ERROR IN FRAME POOL\n");
      Console::puts("i =");
      Console::puti(i);
      Console::puts("   v = ");
      Console::puti(value_array[i]);
      Console::puts("   n =");
      Console::puti(cur_start - 1);
      Console::puts("\n");
      for (;;)
        ; // We throw a fit.
    }
  }
}
// As I am using bitmap to store frame information, only 1 frame should be
// needed for 32MB (max 64MB) space
void test_needed_info_frames(ContFramePool *_pool) {
  assert(_pool->needed_info_frames(512) == (unsigned long)1);
  Console::puts("Info frames needed is 1\n");
}
