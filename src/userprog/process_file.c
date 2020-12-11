#include "userprog/process_file.h"
#include "threads/malloc.h"

struct process_file *
get_process_file_by_fd (struct thread *t, int fd)
{
  struct list *files = &t->open_file_list;
  for (struct list_elem *e = list_begin (files); e != list_end (files); e = list_next (e))
    {
      struct process_file *pfile = list_entry (e, struct process_file, elem);
      if (pfile->fd == fd)
        return pfile;
    }
  return NULL;
}

void
process_file_close (struct process_file *pfile)
{
  list_remove (&pfile->elem);
  file_close (pfile->file);
  free (pfile);
}

void
close_all_open_file (struct thread *t)
{
    struct list *files = &t->open_file_list;
    while (!list_empty (files))
        {
            struct list_elem *e = list_pop_front (files);
            struct process_file *pfile = list_entry (e, struct process_file, elem);
            process_file_close (pfile);
        }
}   
