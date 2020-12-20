#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/malloc.h"
#include "userprog/pagedir.h"
#include "userprog/process_file.h"
#include "userprog/process.h"
#ifdef VM
#include "vm/mmap.h"
#endif
static void syscall_handler (struct intr_frame *f);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int get_syscall_code (struct intr_frame *f);
static void get_args (struct intr_frame *f, int *args, int argc);
static void check_valid_addr (const void *ptr);
static void check_valid_buffer (const void *ptr, unsigned size);
static void check_valid_string (const void *ptr);

void exit (int status);
int open (const char *file_name);
int filesize (int fd);
int read (int fd, void *buffer, unsigned size);
int write (int fd, const void *buffer, unsigned size);
void seek (int fd, unsigned position);
unsigned tell (int fd);
void close (int fd);
#ifdef VM
int mmap(int fd, void* addr);
void munmap(int mapping);
#endif

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int code = get_syscall_code (f);
  int args[3];
  switch (code)
    {
      // void halt ()
      case SYS_HALT:
        shutdown_power_off();
        break;
      // void exit (int status)
      case SYS_EXIT: 
        {
          get_args (f, args, 1);
          exit (args[0]);
          break;
        }
      // pid_t exec (const char *cmd_line)
      case SYS_EXEC:
        {
          get_args (f, args, 1);
          char *cmd_line = (char *)args[0];
          check_valid_string (cmd_line);
          f->eax = process_execute (cmd_line);
          break;
        }
      // int wait (pid_t pid)
      case SYS_WAIT:
        {
          get_args (f,args, 1);
          int pid = args[0];
          f->eax = process_wait (pid);
          break;
        }
      // bool create (const char *file, unsigned initial_size)
      case SYS_CREATE:
        {
          get_args (f, args, 2);
          const char *file = (char *) args[0];
          unsigned initial_size = args[1];
          check_valid_string (file);
          f->eax = filesys_create (file, initial_size);
          break;
        }
      // bool remove (const char *file)
      case SYS_REMOVE:
        {
          get_args (f, args, 1);
          const char *file = (char *) args[0];
          check_valid_string (file);
          f->eax = filesys_remove (file);
          break;
        }
      // int open (const char *file)
      case SYS_OPEN:
        {
          get_args (f, args, 1);
          const char *file = (char *) args[0];
          check_valid_string (file);
          f->eax = open (file);
          break;
        }
      // int filesize (int fd)
      case SYS_FILESIZE:
        {
          get_args (f, args, 1);
          f->eax = filesize (args[0]);
          break;
        }
      // int read (int fd, void *buffer, unsigned size)
      case SYS_READ:
        {
          get_args (f, args, 3);
          int fd = args[0];
          void *buffer = (void *) args[1];
          unsigned size = args[2];
          check_valid_buffer (buffer, size);
          f->eax = read (fd, buffer, size);
          break; 
        }
      // int write (int fd, const void *buffer, unsigned size)
      case SYS_WRITE:
        {
          get_args (f, args, 3);
          int fd = args[0];
          const void *buffer = (void *)args[1];
          unsigned size = args[2];
          check_valid_buffer (buffer, size);
          f->eax = write (fd, buffer, size);
          break;
        }
      // void seek (int fd, unsigned position)
      case SYS_SEEK:
        {
          get_args (f, args, 2);
          int fd = args[0];
          unsigned pos = args[1];
          seek (fd, pos);
          break;          
        }
      // unsigned tell (int fd)
      case SYS_TELL:
        {
          get_args (f, args, 1);
          int fd = args[0];
          f->eax = tell (fd);
          break;
        }
      // void close (int fd)
      case SYS_CLOSE:
        {
          get_args (f, args, 1);
          close (args[0]);
          break;
        }
       // mapid_t mmap(int fd, void* addr)
#ifdef VM
      case SYS_MMAP:
        {
          get_args (f, args, 2);
          int fd = args[0];
          void *addr = (void *) args[1];
          f->eax = mmap (fd, addr);
          break;
        }
      // void munmap(mapid_t mapping)
      case SYS_MUNMAP:
        {
          get_args (f, args, 1);
          int mapid = args[0];
          munmap (mapid);
          break;
        }
#endif      
      default:
        exit (-1);
        break;
    }
}

int
get_syscall_code (struct intr_frame *f)
{
  for (int i = 0; i < 4; i++)
    check_valid_addr (f->esp + i);

  int *ptr = (int *) f->esp;
  return *ptr;
}

void
get_args (struct intr_frame *f, int *args, int argc)
{
  for (int i = 0; i < argc * 4; i++)
    check_valid_addr (f->esp + 4 + i);

  int *ptr;
  for (int i = 0; i < argc; i++)
    {
      ptr = (int *) f->esp + i + 1;
      args[i] = *ptr;
    }
}

#ifdef VM
static int32_t
get_user (const uint8_t *uaddr) {
  // check that a user pointer `uaddr` points below PHYS_BASE
  if (! ((void*) uaddr < PHYS_BASE)) {
    return -1;
  }

  // as suggested in the reference manual, see (3.1.5)
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a" (result) : "m" (*uaddr));
  return result;
}

static int32_t
put_user (uint8_t *udst, uint8_t byte) {
  // check that a user pointer `udst` points below PHYS_BASE
  if (! ((void*)udst < PHYS_BASE)) {
    return false;
  }

  int error_code;

  // as suggested in the reference manual, see (3.1.5)
  asm ("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code;
}

void
check_valid_addr (const void *ptr)
{
  if (get_user (ptr) == -1)
    exit (-1);
}
#else
void
check_valid_addr (const void *ptr)
{
  if (!is_valid_user_vaddr (ptr))
      exit (-1);

  if (pagedir_get_page (thread_current ()->pagedir, ptr) == NULL)
      exit (-1);
}
#endif
void
check_valid_buffer (const void *ptr, unsigned size)
{
  for (unsigned i = 0; i < size; i++)
    check_valid_addr (ptr + i);
}

void
check_valid_string (const void *ptr)
{
  char *str = (char *) ptr;
  do check_valid_addr (str);
  while (*str++);
}

void
exit (int status)
{
  struct thread *cur = thread_current ();
  cur->status_code = status;
  thread_exit ();
}

int 
open (const char *file_name)
{
  struct file *file = filesys_open (file_name);
  if (file == NULL)
    return -1;
  
  struct thread *cur = thread_current ();
  /* memory alloc */
  struct process_file *pfile = malloc (sizeof (struct process_file));
  pfile->file = file;
  pfile->fd = cur->fd++;
  list_push_back (&cur->open_file_list, &pfile->elem);
  return pfile->fd;
}

int
filesize (int fd)
{
  struct process_file *pfile = get_process_file_by_fd (thread_current (), fd);
  if (pfile == NULL)
    return -1;
  return file_length (pfile->file);
}

int
read (int fd, void *buffer, unsigned size)
{
  for (unsigned i = 0; i < size; i++)
    if (put_user (buffer + i, '\0') == -1)
      exit (-1);
  if (fd == 0)
    {
      char *buf = buffer;
      while (size--)
        *buf++ = input_getc();
      return size;
    }
  
  struct process_file *pfile = get_process_file_by_fd (thread_current (), fd);
  if (pfile == NULL)
    return -1;
  return file_read (pfile->file, buffer, size);
}

int
write (int fd, const void *buffer, unsigned size)
{
  if (fd == 1)
    {
      putbuf (buffer, size);
      return size;
    }
  
  struct process_file *pfile = get_process_file_by_fd (thread_current (), fd);
  if (pfile == NULL)
    return -1;
  return file_write (pfile->file, buffer, size);
}

void
seek (int fd, unsigned position)
{
  struct process_file *pfile = get_process_file_by_fd (thread_current (), fd);
  if (pfile == NULL)
    return;
  file_seek (pfile->file, position);
}

unsigned
tell (int fd)
{
  struct process_file *pfile = get_process_file_by_fd (thread_current (), fd);
  if (pfile == NULL)
    return 0;
  return file_tell (pfile->file);
}

void
close (int fd)
{
  if (fd == 0 || fd == 1)
    return;

  struct process_file *pfile = get_process_file_by_fd (thread_current (), fd);
  if (pfile == NULL)
    return;

  process_file_close (pfile);
}

#ifdef VM
int
mmap(int fd, void* addr)
{
  if (fd == 0 || fd == 1)
    return -1;
  
  if (addr == NULL || pg_ofs (addr) != 0)
    return -1;

  struct process_file *pfile = get_process_file_by_fd (thread_current (), fd);
  if (pfile == NULL)
    return -1;
  
  struct file *file = file_reopen(pfile->file);
  off_t file_size = file_length (file);
  if (file_size == 0)
    return -1;
  
  struct thread *cur = thread_current ();
  return mmap_open (cur, file, file_size, addr);
}

void
munmap (int mapid)
{
  struct thread *cur = thread_current ();   

  struct mmap *mmap = get_mmap_by_mapid (cur, mapid);
  if (mmap == NULL)
    return;
  
  mmap_close (cur, mmap);                   
}
#endif