#include "vm/swap.h"
#include "threads/vaddr.h"
#include "devices/block.h"

static struct block *swap_block;
static struct bitmap *swap_available;

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

// the number of possible (swapped) pages.
static size_t swap_size;

void
swap_init (void)
{
  // Initialize the swap disk
  swap_block = block_get_role(BLOCK_SWAP);
  if(swap_block == NULL) 
    PANIC ("Error: Can't initialize swap block");

  swap_size = block_size(swap_block) / SECTORS_PER_PAGE;
  swap_available = bitmap_create(swap_size);
  bitmap_set_all(swap_available, true);
}


size_t
swap_to_block (void *frame)
{
  ASSERT (frame >= PHYS_BASE);

  size_t swap_index = bitmap_scan (swap_available, 0, 1, true);

  ASSERT (swap_index != BITMAP_ERROR);

  for (int i = 0; i < SECTORS_PER_PAGE; i++)
    block_write(swap_block,
                swap_index * SECTORS_PER_PAGE + i,
                frame + (BLOCK_SECTOR_SIZE * i));

  // occupy the slot: available becomes false
  bitmap_set(swap_available, swap_index, false);
  return swap_index;
}


void
swap_from_block (void *frame, size_t swap_index)
{
  ASSERT (frame >= PHYS_BASE);

  // check the swap region
  ASSERT (swap_index < swap_size);
  if (bitmap_test(swap_available, swap_index) == true) {
    PANIC ("invalid block index");
  }

  for (int i = 0; i < SECTORS_PER_PAGE; i++)
    block_read (swap_block,
                swap_index * SECTORS_PER_PAGE + i,
                frame + (BLOCK_SECTOR_SIZE * i));

  bitmap_set(swap_available, swap_index, true);
}

void
swap_free (size_t swap_index)
{
  ASSERT (swap_index < swap_size);
  if (bitmap_test(swap_available, swap_index) == true)
    PANIC ("invalid block index");

  bitmap_set(swap_available, swap_index, true);
}
