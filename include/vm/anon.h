#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
#include "lib/kernel/bitmap.h"
struct page;
enum vm_type;

struct anon_page {
    size_t bit_idx;
};

extern struct swap_table swap_t;
void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
