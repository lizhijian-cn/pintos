# USERPROG
pintos实验二的文档。

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