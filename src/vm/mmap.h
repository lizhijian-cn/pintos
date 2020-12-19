#include "filesys/file.h"
#include "threads/thread.h"

struct mmap
  {
    int mapid;
    struct file *file;
    off_t file_size;
    void *upage;
    struct list_elem elem;
  };


struct mmap *get_mmap_by_mapid (struct thread *t, int mapid);
int mmap_open (struct thread *t, struct file *file, off_t file_size, void *upage);
void mmap_close (struct thread *t, struct mmap *mmap);
void close_all_mmap (struct thread *t);