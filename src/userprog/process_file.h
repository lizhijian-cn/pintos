#include "threads/thread.h"
#include "filesys/file.h"

struct process_file
  {
    struct file *file;
    int fd;
    struct list_elem elem;
  };

struct process_file *get_process_file_by_fd (struct thread *t, int fd);
void process_file_close (struct process_file *pfile);
void close_all_open_file (struct thread *t);