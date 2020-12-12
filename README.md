# USERPROG
pintos实验二的文档。

# 写在前面
实验二考察的主要是一系列的系统调用，核心内容是与进程有关的`exec`, `wait`, `exit`。

由于`exec`, `wait`的调用者都是父进程，所以是父进程依赖子进程，那么整个设计中，应该是由父进程来维护两者之间的关系，子进程不应该维护任何有关父进程的信息。

在实际设计中，为了简化load子进程的实现，子进程会维护一个指向父进程的指针，用于block父进程，并将自己添加到父进程的子进程队列中。

现在归纳下父子的职责。

* 子进程
    * `start_process`
        * append(cur->parent->child_process_list, cur)
        * update parent->load_success
        * sema_up(cur->parent->load_sema)
    * `process_exit` 
        * put (tid, status_code) in global map
        * sema_up(cur->wait_sema)

* 父进程
    * `process_execute`
        * sema_down(cur->load_sema)
    * `process_wait`
        * look up child process and sema_down(child->wait_sema)
        * get child status code from global map

* 共同职责
    * `start_process`
        * open itself
    * `process_exit` 
        * close itself
        * close all opened files

## 先跑起来
为了通过一小部分测试点，我们需要做这些工作
* 传递`executable name`而不是`raw filename`给`thread_create`和`load`中的`filesys_open`
* 解析参数并设置在栈中
* 简单实现`process_wait`，让父进程不那么快结束
* 实现系统调用`exit`和向屏幕输出`write`

### 


## 非法内存
非法内存分为两种，一种是不属于用户进程空间的地址，一种是属于用户进程空间，但未分配的地址（野指针）
使用了三个函数：
```c
void
check_valid_addr (const void *ptr)
{
  if (ptr == NULL || ptr >= PHYS_BASE || ptr < (void *) 0x08048000)
    exit (-1);

  if (pagedir_get_page (thread_current ()->pagedir, ptr) == NULL)
    exit (-1);
}

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
```

## 内存泄漏

free process_file when close file
close all files when thread exit
fn_copy
thread本身的资源

## exec & wait
先明确一些特殊情况
* 使用strtok_r前必须拷贝字符串，因为strtok_r会更改输入的字符串，如果输入串的所在的用户页是只读的，会报错！！！

* 当子进程load失败时，父进程的process_execute应该返回-1，所以process_execute必须要阻塞，直到子进程load返回

* 接上条，子进程load失败时，子进程的status_code为-1

* 同样地，子进程因为访问非法内存抛异常时，status_code为-1

* wait同一个子进程两次时，第二次返回-1（所以子进程退出时，记得把自己从父进程的`child_process_list`中删除


新增一些字段
由父进程负责的有
* struct list child_process_list;
* bool load_success;
* struct semaphore load_sema;

main 线程需要 初始化这些

由子进程负责的有
* struct list_elem process_elem;
* struct semaphore wait_sema;

全局map存放status_code（暂时用10000长度的数组）

子进程没有任何阻塞的操作，父进程在wait子进程时，会根据child_tid遍历自己的子进程列表

那么权衡的地方来了，有两种情况会导致父进程找不到子进程
* 子进程已经结束，从父进程列表删除了（包括第二次wait）
* 给的tid是非法的

还有个问题，即便得到了子进程的指针，当父进程去获取其状态码时，子进程很有可能已经被释放了。

可以通过保存所有进程的状态来解决这些问题
```c
#define MAX_PROCESS_COUNT 10000
enum PID_STATUS
  {
    INVALID,
    NONWAITED,
    WAITED
  };
static enum PID_STATUS process_status[MAX_PROCESS_COUNT];
static int process_status_code[MAX_PROCESS_COUNT];

int
process_wait (tid_t child_tid) 
{
  struct thread *cur = thread_current ();
  if (child_tid > MAX_PROCESS_COUNT || process_status[child_tid] == INVALID || process_status[child_tid] == WAITED)
    return -1;
  
  process_status[child_tid] = WAITED;
  struct thread *t = get_thread_by_tid (cur, child_tid);
  if (t != NULL)
    sema_down (&t->wait_sema);
  return process_status_code[child_tid];
}
```
正确做法是用个哈希表或者堆，但我为了省事，直接判断大于10000的pid是非法的，大家懂这个意思就好

这时候只剩下5个点（rox-*和sync-*），no-vm过不了找内存泄漏
![](https://raw.githubusercontent.com/lizhijian-cn/static/master/img/20201212031622.png)

## rox-*
```c
  if (success)
    {
      file_deny_write (file);
      thread_current ()->self_file = file;
    }
  else
    {
      file_close (file);
      thread_current ()->self_file = NULL;
    }
```

```c
  if (cur->self_file != NULL)
    {
      file_allow_write (cur->self_file);
      file_close (cur->self_file);
    }
```

## sync-*

对filesys里所有操作加锁

![](https://raw.githubusercontent.com/lizhijian-cn/static/master/img/20201212042757.png)