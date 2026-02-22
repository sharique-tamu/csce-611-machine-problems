#include "assert.H"
#include "console.H"
#include "exceptions.H"
#include "page_table.H"
#include "paging_low.H"

#define MB *(0x1 << 20)
#define KB *(0x1 << 10)

PageTable *PageTable::current_page_table = nullptr;
unsigned int PageTable::paging_enabled = 0;
ContFramePool *PageTable::kernel_mem_pool = nullptr;
ContFramePool *PageTable::process_mem_pool = nullptr;
unsigned long PageTable::shared_size = 0;

void PageTable::init_paging(ContFramePool *_kernel_mem_pool,
                            ContFramePool *_process_mem_pool,
                            const unsigned long _shared_size) {
  kernel_mem_pool = _kernel_mem_pool;
  process_mem_pool = _process_mem_pool;
  shared_size = _shared_size;
  assert(shared_size == 4 MB);
  Console::puts("Initialized Paging System\n");
}

PageTable::PageTable() {
  // Initialize page directory
  unsigned long frame = kernel_mem_pool->get_frames(1);
  page_directory = (unsigned long *)(frame * 4 KB);

  // Initialize first page table for direct mapping of shared size
  frame = kernel_mem_pool->get_frames(1);
  unsigned long address = 0;

  unsigned long *page_table = (unsigned long *)(frame * 4 KB);

  for (int i = 0; i < shared_size / (4 KB); i++) {
    // Enabling R/W bit and Valid bit
    page_table[i] = address | 3;
    address += 4 KB;
  }

  page_directory[0] = (unsigned long)page_table | 3;
  for (int i = 1; i < 1024; i++) {
    // Marking them as read write and invalid
    page_directory[i] = 0 | 2;
  }

  Console::puts("Constructed Page Table object\n");
}

void PageTable::load() {
  write_cr3((unsigned long)page_directory);
  Console::puts("Loaded page table\n");
}

void PageTable::enable_paging() {
  assert(false);
  Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS *_r) {
  assert(false);
  Console::puts("handled page fault\n");
}
