#include "vm/sup-page-table.h"
#ifdef DEBUG
#include <stdio.h>
#endif
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/frame-table.h"
#include "vm/swap.h"

static unsigned
spte_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *spte = hash_entry (elem, struct sup_page_table_entry, hash_elem);
  return hash_bytes(&spte->upage, sizeof (spte->upage));
}

static bool
spte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct sup_page_table_entry *a_spte = hash_entry (a, struct sup_page_table_entry, hash_elem);
  struct sup_page_table_entry *b_spte = hash_entry (b, struct sup_page_table_entry, hash_elem);
  return a_spte->upage < b_spte->upage;
}

static void
spte_destroy_func(struct hash_elem *elem, void *aux UNUSED)
{
  struct sup_page_table_entry *spte = hash_entry (elem, struct sup_page_table_entry, hash_elem);
  spt_free_spte (spte);
}

void
spt_init (struct hash *spt)
{
  hash_init (spt, spte_hash_func, spte_less_func, NULL); 
}

void
spt_destroy (struct hash *spt)
{
  hash_destroy (spt, spte_destroy_func);
}

struct sup_page_table_entry *
spt_lookup (struct hash *spt, void *upage)
{
  ASSERT (pg_ofs (upage) == 0);

  struct sup_page_table_entry spte;
  spte.upage = upage;

  struct hash_elem *elem = hash_find (spt, &spte.hash_elem);
  return elem != NULL ? hash_entry (elem, struct sup_page_table_entry, hash_elem) : NULL;
}

struct sup_page_table_entry *
spt_get_spte (struct hash *spt, void *upage)
{
  ASSERT (pg_ofs (upage) == 0);

  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));

  spte->status = EMPTY;
  spte->upage = upage;
  spte->frame = NULL;

  ASSERT (hash_insert (spt, &spte->hash_elem) == NULL);
  return spte;
}

struct sup_page_table_entry *
spt_get_file_spte (struct hash *spt, void *upage, struct file * file, off_t offset, 
                   uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT (pg_ofs (upage) == 0);

  struct sup_page_table_entry *spte = malloc (sizeof (struct sup_page_table_entry));

  spte->status = FILE;
  spte->upage = upage;
  spte->frame = NULL;

  spte->file = file;
  spte->offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;

  ASSERT (hash_insert (spt, &spte->hash_elem) == NULL);
  return spte;
}

void
spt_free_spte (struct sup_page_table_entry *spte)
{
  if (spte->frame != NULL)
    {
      ASSERT (spte->status == FRAME);
      ft_free_frame (spte->frame, false);
    }
  else if(spte->status == SWAP)
    swap_free (spte->swap_index);
  
  free (spte);
}

void
spt_convert_spte_to_swap (struct hash *spt, void *upage, size_t swap_index)
{
  ASSERT (pg_ofs (upage) == 0);

  struct sup_page_table_entry *spte = spt_lookup (spt, upage);
  ASSERT (spte != NULL);
  ASSERT (spte->status != SWAP);

#ifdef DEBUG
  printf ("debug: spte: %p, %d, frame: %p, upage: %p, swap_i: %d\n", spte, spte->status, spte->frame, spte->upage, swap_index);
#endif

  spte->status = SWAP; 
  spte->swap_index = swap_index;
  spte->frame = NULL;
}