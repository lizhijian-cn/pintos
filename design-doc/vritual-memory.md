            +---------------------------+
            |          CS 140          |
            | PROJECT 3: VIRTUAL MEMORY |
            |      DESIGN DOCUMENT      |
            +---------------------------+

## GROUP

> Fill in the names and email addresses of your group members.

FirstName LastName <email@domain.example>
FirstName LastName <email@domain.example>
FirstName LastName <email@domain.example>

## PRELIMINARIES

> If you have any preliminary comments on your submission, notes for the
> TAs, or extra credit, please give them here.

> Please cite any offline or online sources you consulted while
> preparing your submission, other than the Pintos documentation, course
> text, lecture notes, and course staff.

https://github.com/chengqidexue/yuan-OS

# PAGE TABLE MANAGEMENT

## DATA STRUCTURES

> A1: Copy here the declaration of each new or changed `struct' or `struct' member, global or static variable, `typedef', or
> enumeration. Identify the purpose of each in 25 words or less.

```c
// threads/thread.h
struct thread {
  struct hash spt;            // 进程的Supplemental page table
  struct list mmap_list;      // 进程映射的文件列表
  int mapid;                  // 进程维护的分配给memory mapping的id，每分配一次都递增
};
```
```c
// vm/frame-table.c

static struct list frame_list;        // 帧表的链表形式，主要用于二次时钟算法
static struct hash frame_table;       // 帧表的哈希表形式， 主要用于释放帧
static struct lock frame_lock;        // 帧表的全局锁，merge-*测试点用到
static struct list_elem *clock_elem;  // 二次时钟算法指向的当前帧

struct frame_table_entry {
  void *frame;                // 物理内存（帧）
  void *upage;                // 用户虚拟内存
  struct thread *owner;       // 拥有该帧的线程

  struct list_elem elem;      
  struct hash_elem hash_elem;
};
```
```c
// vm/sup-page-table.h

// 指明页的类型
enum spte_status {
  ON_FRAME,                   // 已经为该页分配内存
  EMPTY,                      // 除了文件和交换区以外的“普通”页
  IN_SWAP,                    // 该页已被evicted，被交换到了交换区
  IN_FILE                     // 该页内容还在文件里，未被load
};

struct sup_page_table_entry {
  enum spte_status status;    // 指明该页的类型

  void *upage;                // 该页对应的用户虚拟内存
  void *frame;                // 为该页分配的帧

  // spte_status == IN_SWAP
  size_t swap_index;          // 如果该页被交换到交换区，其内容位于交换区的位置

  // spte_status == IN_FILE
  struct file *file;          // 该页内容来源的文件指针
  off_t offset;               // 内容位于文件中的偏移
  uint32_t read_bytes;        // 从文件中读取的字节数
  uint32_t zero_bytes;        // 用于填补
  bool writable;

  struct hash_elem hash_elem;
};
```

## ALGORITHMS

> A2: In a few paragraphs, describe your code for accessing the data
> stored in the SPT about a given page.

我们使用了一种结构`sup-page-table-entry(spte)`来管理所有有效的虚拟内存页。每个已分配帧的虚拟内存页的`frame`字段都不为 NULL，否则字段`status`会著名这个页的数据为空，还是存放在交换区和文件中。

所以在 page fault 时，会先判断该页是否有 spte，如果没有，判断错误地址是否是因为栈增长导致的。如果是，则为其分配一个表示空白页的 spte。

接下来就是根据 spte 的 status 的不同，执行相应的处理。
* 如果是空白页，那么什么都不做（也可以 memset 设为 0，表示局部内存初始化为 0）
* 如果数据在交换区，那么根据 swap_index 从交换区载入
* 如果数据在文件，那么根据相关字段从文件读取
```c
  if (spte == NULL)
    {
      bool grow_stack_contiguous = fault_addr >= (f->esp - 32);
      if (!is_valid_user_vaddr (fault_addr) || !grow_stack_contiguous)
        goto failed;
      
      spte = spt_get_spte (&cur->spt, upage);
      ASSERT (spte != NULL);
    }
  bool writable = true;
  void *frame = ft_get_frame (upage);
  switch (spte->status)
    {
      case EMPTY:
        memset (frame, 0, PGSIZE);
        break;
      case IN_SWAP:
        swap_from_block (frame, spte->swap_index);
        break;
      case IN_FILE:
        file_seek (spte->file, spte->offset);
        if (file_read (spte->file, frame, spte->read_bytes) != (off_t) spte->read_bytes)
          {
            ft_free_frame (frame, true, false);
            return;
          }
        writable = spte->writable;
        memset (frame + spte->read_bytes, 0, spte->zero_bytes);
        break;
      case ON_FRAME:
      default:
        NOT_REACHED();
    }
  spte->status = ON_FRAME;
  spte->frame = frame;
  bool access = pagedir_set_page (cur->pagedir, upage, frame, writable);
```

> A3: How does your code coordinate accessed and dirty bits between
> kernel and user virtual addresses that alias a single frame, or
> alternatively how do you avoid the issue?

脏位和访问位会由CPU自己设置，不需要我们手动管理。

## SYNCHRONIZATION

> A4: When two user processes both need a new frame at the same time,
> how are races avoided?

所有进程都通过 ft_get_frame 来获取一个被管理的帧，因此，只需要在调用时加锁，在退出时解锁，就可避免两个进程同时申请分配帧。

## RATIONALE 

> A5: Why did you choose the data structure(s) that you did for
> representing virtual-to-physical mappings?

page fault 是一种很常见的中断，因此需要一种根据虚拟内存快速查找对应 spte 的数据结构，因此我们选择哈希表来存储每一个spte。

同时，page fault 和 释放 spte 的时候，OS 需要根据 spte 的不同类型来执行不同的操作，因此，spte 要储存对应的信息。比如对于交换区，要保存 swap_index。对于文件，要保存文件指针、偏移量、read_bytes。

# PAGING TO AND FROM DISK

## DATA STRUCTURES

> B1: Copy here the declaration of each new or changed `struct' or `struct' member, global or static variable, `typedef', or
> enumeration. Identify the purpose of each in 25 words or less.
```c
// threads/thread.h
struct thread {
  struct list mmap_list;      // 进程映射的文件列表
  int mapid;                  // 进程维护的分配给memory mapping的id，每分配一次都递增
};
```
```c
// vm/mmap.h
struct mmap {
  int mapid;                          // 一个线程映射的内存的序号
  struct file *file;                  // 该内存映射的文件
  off_t file_size;                    // 文件开始映射的偏移
  void *upage;                        // 文件映射到的虚拟地址
  struct list_elem elem;
};
```

## ALGORITHMS

> B2: When a frame is required but none is free, some frame must be
> evicted. Describe your code for choosing a frame to evict.

我们采用了二次时钟算法，跳过了 accessed bit 和 Pinned 为 true的帧。

> B3: When a process P obtains a frame that was previously used by a
> process Q, how do you adjust the page table (and any other data
> structures) to reflect the frame Q no longer has?

* 清楚pagedir的对应页

  `pagedir_clear_page (fte->owner->pagedir, fte->upage);`

* 释放帧
  `ft_free_frame (fte->frame, true, true);`

* 将 spte 的帧置为 NULL
  `spte->frame = NULL;`

> B4: Explain your heuristic for deciding whether a page fault for an
> invalid virtual address should cause the stack to be extended into
> the page that faulted.

不会，任何非法虚拟内存的 page fault 都会返回 -1。

---- SYNCHRONIZATION ----

> B5: Explain the basics of your VM synchronization design. In
> particular, explain how it prevents deadlock. (Refer to the
> textbook for an explanation of the necessary conditions for
> deadlock.)

我们对全局的帧表使用了锁，在分配帧和释放帧的开头和结尾加锁和放锁。因此我们的设计不存在死锁。

> B6: A page fault in process P can cause another process Q's frame
> to be evicted. How do you ensure that Q cannot access or modify
> the page during the eviction process? How do you avoid a race
> between P evicting Q's frame and Q faulting the page back in?

我们会在牺牲 Q 的帧之前先调用 pagedir_clear_page。这样可以保证只要 Q 能够成功access/modify 一个页时，它对应的帧一定时有效的，否则就是出发 page fault，重新获得一个帧。

> B7: Suppose a page fault in process P causes a page to be read from
> the file system or swap. How do you ensure that a second process Q
> cannot interfere by e.g. attempting to evict the frame while it is
> still being read in?

每个帧都由一个 pinned 字段，只有在完成获取数据后，pinned 才会被设为 true，这么帧才会被加入可能被牺牲的帧队列里。

> B8: Explain how you handle access to paged-out pages that occur
> during system calls. Do you use page faults to bring in pages (as
> in user programs), or do you have a mechanism for "locking" frames
> into physical memory, or do you use some other design? How do you
> gracefully handle attempted accesses to invalid virtual addresses?

我们的实现是第一种，在需要的时候换入页。为每一个虚拟地址生成一个struct page，若其被swap out，则调用pagedir_clear_page，以便在下次CPU使用时产生一个page fault。在生成一个page之前，先检查该用户虚拟地址是否已经有了一个page，若已经有了，说明这个虚拟地址已经被使用过了（mmap/data segment/code segment/stack segment），是无效的。

---- RATIONALE ----

> B9: A single lock for the whole VM system would make
> synchronization easy, but limit parallelism. On the other hand,
> using many locks complicates synchronization and raises the
> possibility for deadlock but allows for high parallelism. Explain
> where your design falls along this continuum and why you chose to
> design it this way.

我们并不行引入过多的锁，所以只为全局的帧表和交换区引入了两个锁。在仔细思考后，我们发现只要保证在palloc_get_page不为NULL时，获得的帧是合法的，就不会导致同步错误的问题。所以，尽管并没有为每个帧使用一个锁，我们的程序依然能够通过测试点。

# MEMORY MAPPED FILES

## DATA STRUCTURES

> C1: Copy here the declaration of each new or changed `struct' or `struct' member, global or static variable, `typedef', or
> enumeration. Identify the purpose of each in 25 words or less.

```c
// threads/thread.h
struct thread {
  struct list mmap_list;      // 进程映射的文件列表
  int mapid;                  // 进程维护的分配给memory mapping的id，每分配一次都递增
};
```
```c
// vm/mmap.h
struct mmap {
  int mapid;                          // 一个线程映射的内存的序号
  struct file *file;                  // 该内存映射的文件
  off_t file_size;                    // 文件开始映射的偏移
  void *upage;                        // 文件映射到的虚拟地址
  struct list_elem elem;
};
```

## ALGORITHMS 

> C2: Describe how memory mapped files integrate into your virtual
> memory subsystem. Explain how the page fault and eviction
> processes differ between swap pages and other pages.

page fault时，swap pages 是从交换区换入，file Pages 是从文件中读入。
eviction 时两者没有区别。

> C3: Explain how you determine whether a new file mapping overlaps
> any existing segment.

在file mapping 时，会为每个页生成一个 spte，因此，如果存在 overlap 的情况，会导致为通过一个页生成多个 spte 的情况。所以，只要在映射时检查该虚拟地址是否已被映射过，就可以解决 overlap 的问题。

---- RATIONALE ----

> C4: Mappings created with "mmap" have similar semantics to those of
> data demand-paged from executables, except that "mmap" mappings are
> written back to their original files, not to swap. This implies
> that much of their implementation can be shared. Explain why your
> implementation either does or does not share much of the code for
> the two situations.

在我们的实现中，mmap 和 懒加载程序的实现是完全一样的。在被牺牲时都选择换出交换区，在unmap / 进程结束释放时都选择根据是否 dirty 写入文件。

               SURVEY QUESTIONS
               ================

Answering these questions is optional, but it will help us improve the
course in future quarters. Feel free to tell us anything you
want--these questions are just to spur your thoughts. You may also
choose to respond anonymously in the course evaluations at the end of
the quarter.

> In your opinion, was this assignment, or any one of the three problems
> in it, too easy or too hard? Did it take too long or too little time?

> Did you find that working on a particular part of the assignment gave
> you greater insight into some aspect of OS design?

> Is there some particular fact or hint we should give students in
> future quarters to help them solve the problems? Conversely, did you
> find any of our guidance to be misleading?

> Do you have any suggestions for the TAs to more effectively assist
> students, either for future quarters or the remaining projects?

> Any other comments?
