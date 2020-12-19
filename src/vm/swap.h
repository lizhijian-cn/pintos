#include <bitmap.h>

void swap_init (void);
size_t swap_to_block (void *upage);
void swap_from_block (void *upage, size_t swap_index);
void swap_free (size_t swap_index);