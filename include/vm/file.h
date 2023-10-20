#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	struct file* file;
    size_t read_bytes;
    size_t zero_bytes;
    long offset;
    bool writable;
	void* init_mmaped_va;
    size_t page_cnts;

    // struct file* file;
    // size_t page_read_bytes;
    // size_t page_zero_bytes;
    // off_t offset;
    // bool writable;
    // void* init_mapped_va;
    // size_t page_cnts;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, long length, int writable,
		struct file *file, long offset);
void do_munmap (void *va);
#endif
