#ifndef VM_ANON_H
#define VM_ANON_H
#include "devices/disk.h"
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
    disk_sector_t swap_disk_sector_start;
};

void vm_anon_init(void);
bool anon_initializer(struct page *page, enum vm_type type, void *kva);

#endif
