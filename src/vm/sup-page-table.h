#include <hash.h>
#include "filesys/file.h"

enum spte_status
  {
    ON_FRAME,
    EMPTY,
    IN_SWAP,
    IN_FILE
  };

struct sup_page_table_entry
  {
    enum spte_status status;

    void *upage;
    void *frame;

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

void spt_free_spte (struct hash *spt, void *upage);
void spt_free_file_spte (struct hash *spt, void *upage, struct file *file, off_t offset, size_t read_bytes, uint32_t *pagedir);
void spt_convert_spte_to_swap (struct hash *spt, void *upage, size_t swap_index);