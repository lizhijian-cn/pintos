#include "vm/frame-table.h"
#ifdef DEBUG
#include <stdio.h>
#endif
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"
#include "vm/sup-page-table.h"

static struct list frame_list;
static struct hash frame_table;

static struct list_elem *clock_elem;

struct frame_table_entry
  {
    void *frame;
    void *upage;
    struct thread *owner;

    struct list_elem elem;
    struct hash_elem hash_elem;
  };

static unsigned
fte_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame_table_entry *entry = hash_entry (elem, struct frame_table_entry, hash_elem);
  return hash_bytes(&entry->frame, sizeof (entry->frame));
}

static bool
fte_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame_table_entry *a_entry = hash_entry (a, struct frame_table_entry, hash_elem);
  struct frame_table_entry *b_entry = hash_entry (b, struct frame_table_entry, hash_elem);
  return a_entry->frame < b_entry->frame;
}

void
ft_init (void)
{
  list_init (&frame_list);
  hash_init (&frame_table, fte_hash_func, fte_less_func, NULL);
  clock_elem = NULL;
}

/*
void 
ft_destroy (void)
{
  while (!list_empty (&frame_list))
    {
      struct list_elem *elem = list_pop_front (&frame_list);
      struct frame_table_entry *fte =  list_entry (elem, struct frame_table_entry, elem);
      ft_free_frame (fte);
    }
}
*/
static struct frame_table_entry *
clock_next (void)
{
  ASSERT (!list_empty (&frame_list));

  if (clock_elem == NULL || clock_elem == list_end (&frame_list)->prev)
    clock_elem = list_begin (&frame_list);
  else
    clock_elem = list_next (clock_elem);
  
  ASSERT (clock_elem != NULL);
  return list_entry (clock_elem, struct frame_table_entry, elem);
}

static struct frame_table_entry *
evict (uint32_t *pagedir)
{
  size_t n = list_size (&frame_list);
  ASSERT (n != 0);

  for (size_t i = 0; i < n + n; i++)
    {
      struct frame_table_entry *fte = clock_next ();
      if (pagedir_is_accessed (pagedir, fte->upage))
        {
          pagedir_set_accessed (pagedir, fte->upage, false);
          continue;
        }
      return fte;
    }
  
  NOT_REACHED();
}

static struct frame_table_entry *
ft_lookup (void *frame)
{
  struct frame_table_entry fte;
  fte.frame = frame;
  struct hash_elem *elem = hash_find (&frame_table, &fte.hash_elem);
  return elem != NULL ? hash_entry (elem, struct frame_table_entry, hash_elem) : NULL;
}

void *
ft_get_frame (void *upage)
{
  void *frame = palloc_get_page (PAL_USER);

  if (frame == NULL)
    {
      struct frame_table_entry *fte = evict (thread_current ()->pagedir);
      pagedir_clear_page (fte->owner->pagedir, fte->upage);
      
      size_t swap_index = swap_to_block (fte->frame);
      spt_convert_spte_to_swap (&fte->owner->spt, fte->upage, swap_index);
      ft_free_frame (fte->frame, true);

      frame = palloc_get_page (PAL_USER);
      ASSERT (frame != NULL);
    }
#ifdef DEBUG
  // printf("ft_get_frame(): get %p\n", frame);
#endif
  struct frame_table_entry *fte = malloc (sizeof (struct frame_table_entry));

  fte->frame = frame;
  fte->upage = upage;
  fte->owner = thread_current ();

  list_push_back (&frame_list, &fte->elem);
  hash_insert (&frame_table, &fte->hash_elem);
  return frame;
}

void
ft_free_frame (void *frame, bool free_frame)
{
  struct frame_table_entry *fte = ft_lookup (frame);
  ASSERT (fte != NULL);

  list_remove (&fte->elem);
  hash_delete (&frame_table, &fte->hash_elem);
  if (free_frame)
    {
      palloc_free_page (fte->frame);
#ifdef DEBUG
      // printf("ft_free_frame(): free %p\n", frame);
#endif
    }
  free (fte);
}