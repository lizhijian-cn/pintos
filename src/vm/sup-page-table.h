#include <hash.h>
#include "filesys/file.h"

enum spte_status
  {
    FRAME,
    EMPTY,
    SWAP,
    FILE
  };

struct sup_page_table_entry
  {
    enum spte_status status;

    void *upage;
    void *frame;

    struct thread *owner;

    size_t swap_index;

    struct file *file;
    off_t offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    struct hash_elem hash_elem;
  };

void spt_init (struct hash *spt);
void spt_destroy (struct hash *spt);

struct sup_page_table_entry *spt_lookup (struct hash *spt, void *upage);

struct sup_page_table_entry *spt_get_spte (struct hash *spt, void *upage);
struct sup_page_table_entry *spt_get_file_spte (struct hash *spt, void *upage, struct file * file, off_t offset, 
                                                uint32_t read_bytes, uint32_t zero_bytes, bool writable);

void spt_free_spte (struct sup_page_table_entry *spte);

void spt_convert_spte_to_swap (struct hash *spt, void *upage, size_t swap_index);