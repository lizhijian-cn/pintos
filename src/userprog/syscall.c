#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static int get_syscall_code (struct intr_frame *f);
static void get_args (struct intr_frame *f, int *args, int argc);
static void check_valid_addr (const void *ptr);

void exit (int status);
int write (int fd, const void *buffer, unsigned size);

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  int code = get_syscall_code (f);
  int args[3];
  switch (code)
    {
      // void exit (int status)
      case SYS_EXIT: 
        {
          get_args (f, args, 1);
          exit (args[0]);
          break;
        }
      // int write (int fd, const void *buffer, unsigned size)
      case SYS_WRITE:
        {
          get_args (f, args, 3);
          int fd = args[0];
          const void *buffer = (void *)args[1];
          unsigned size = args[2];
          f->eax = write (fd, buffer, size);
          break;
        }
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

void
check_valid_addr (const void *ptr)
{
  if (ptr == NULL || ptr >= PHYS_BASE || ptr < (void *) 0x08048000)
    exit (-1);

  // if (pagedir_get_page (thread_current ()->pagedir, ptr) == NULL)
  //   exit (-1);
}

void
exit (int status)
{
  struct thread *cur = thread_current ();
  cur->status_code = status;
  printf ("%s: exit(%d)\n", cur->name, cur->status_code);
  thread_exit ();
}

int
write (int fd, const void *buffer, unsigned n)
{
  if (fd == 1)
    {
      putbuf (buffer, n);
      return n;
    }
  return -1;
}