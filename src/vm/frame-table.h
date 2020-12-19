#include "threads/thread.h"

void ft_init (void);
// unused, frame will be destory when process exit
// void ft_destroy (void);

void *ft_get_frame (void *upage);
void ft_free_frame (void *frame, bool free_frame, bool caller_is_get_frame);