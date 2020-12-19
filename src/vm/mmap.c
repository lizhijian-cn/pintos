#include "vm/mmap.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "vm/sup-page-table.h"

struct mmap *
get_mmap_by_mapid(struct thread *t, int mapid)
{
  struct list *mmap_list = &t->mmap_list;
  for (struct list_elem *e = list_begin(mmap_list); e != list_end(mmap_list); e = list_next(e))
    {
      struct mmap *mmap = list_entry(e, struct mmap, elem);
      if (mmap->mapid == mapid)
        return mmap;
    }
  return NULL;
}

int
mmap_open (struct thread *t, struct file *file, off_t file_size, void *upage)
{
  for (off_t offset = 0; offset < file_size; offset += PGSIZE)
    {
      void *addr = upage + offset;

      size_t read_bytes = (offset + PGSIZE < file_size ? PGSIZE : file_size - offset);
      size_t zero_bytes = PGSIZE - read_bytes;

      spt_get_file_spte (&t->spt, addr, file, offset, read_bytes, zero_bytes, true);
    }

  struct mmap *mmap = malloc (sizeof (struct mmap));
  mmap->mapid = t->mapid++;
  mmap->file = file;
  mmap->file_size = file_size;
  mmap->upage = upage;
  list_push_back (&t->mmap_list, &mmap->elem);
  return mmap->mapid;
}

void
mmap_close (struct thread *t, struct mmap *mmap)
{
  off_t file_size = mmap->file_size;
  for (off_t offset = 0; offset < file_size; offset += PGSIZE)
    {
      void *addr = mmap->upage + offset;
      size_t read_bytes = offset + PGSIZE < file_size ? PGSIZE : file_size - offset;
      spt_free_file_spte (&t->spt,  addr, mmap->file, offset, read_bytes, t->pagedir);
    }
  list_remove (&mmap->elem);
  free(mmap->file);
  free (mmap); 
}

void
close_all_mmap (struct thread *t)
{
  struct list *mmaps = &t->mmap_list;
  while (!list_empty (mmaps))
    {
      struct list_elem *e = list_front (mmaps);
      struct mmap *mmap = list_entry(e, struct mmap, elem);
      mmap_close (t, mmap);
    }
}

