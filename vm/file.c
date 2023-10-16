/* file.c: Implementation of memory backed file object (mmaped object). */
/* memory backed file 개체 구현 파일file.c */

// clang-format off
#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include <hash.h> // SPT 해시테이블을 위해서 추가

// #define VM
// clang-format on

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
    .swap_in = file_backed_swap_in,
    .swap_out = file_backed_swap_out,
    .destroy = file_backed_destroy,
    .type = VM_FILE,
};

/* The initializer of file vm */
/* file vm 초기화 함수 */
void vm_file_init(void) {

    /* File 전용 페이지에 Data를 채워서 초기화 해주는 함수 */

    /* @@@@@@@@@@ Todo @@@@@@@@@@ */
}

/* Initialize the file backed page */
/* file backed page 초기화 */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva) {

    /* File-backed 페이지를 초기화 하는 전용 함수.
       vm_alloc_page_with_initializer()에서 사용될 함수로 보임.
       해당 페이지의 operations 멤버를 file_ops로 정해주는 등. */

    page->operations = &file_ops;
    struct file_page *file_page = &page->file;
    return true;
}

/* Swap in the page by read contents from the file. */
/* 파일을 swap in 시켜줌 */
static bool file_backed_swap_in(struct page *page, void *kva) {

    /* 파일에서 데이터를 읽어온 뒤 해당 해당 페이지를 DRAM에 로딩하는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
/* 파일을 swap out함 */
static bool file_backed_swap_out(struct page *page) {

    /* DRAM에서 해당 페이지를 제거한 뒤 디스크에 변경사항을 저장하는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
/* 파일을 destory함 */
static void file_backed_destroy(struct page *page) {

    /* File-backed 페이지와 관련된 리소스를 전부 Free해주는 함수. 파일을 닫는 등의 행위도 일어나야 함. */
    /* 열었던 file만 닫아주고, file 변경사항은 디스크에 write 해주기 */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
/* mmap 함수 구현 */
/* fd로 열린 file을 offset부터 length만큼 읽어서 addr에 lazy하게 매핑하겠다. */
void *do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset) {

    /* 파일을 메모리에 매핑하는 함수. 유저의 VA, 바이트 크기, Write 가능여부, 파일 포인터, 그리고 Offset을 활용. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
    
    // 가상 주소 addr부터, addr + length까지 매핑됨
    // 매핑되는 값은 file의 offset지점부터 offset + length지점까지임
    // open할 때 어떻게 하는지 보기

    /*
        length를 0부터 시작해서 PGSIZE를 더해가며 page initialize 수행
    */
   // 읽을 길이가 0이면 NULL
   if (length == 0) {
        return NULL;
   }

    // 매핑될 시작 가상주소가 page_aligned 아니면 NULL
   if ((char)addr % PGSIZE != 0) {
        return NULL;
   }

    // 그 외의 경우에 mmap 수행
    struct thread* t = thread_current();
    size_t temp_len = length; // len > 0 만족
    char* temp_addr = addr; // page_aligned 만족
    size_t temp_offset = offset;
    struct file_info* f_info;

    // 매핑할게 남아 있을 때,
    while (temp_len > 0) {
        // 일단 해당 페이지가 이미 매핑되어 있는지 확인
        if (spt_find_page(&t->spt, temp_addr)) {
            // 매핑 되어있을 때, 지금까지 만들었던 페이지 구조체 삭제 및 NULL 반환
            for (char va = (char)addr; va < temp_addr; va += PGSIZE) {
                struct page* page = spt_find_page(&t->spt, va);
                vm_dealloc_page(page);
            }
            return NULL;
        }

        // 매핑 안 되어 있을 때,

        f_info = (struct file_info*)calloc(1, sizeof(struct file_info));
        f_info->file = file;
        f_info->offset = temp_offset;

        // PGSIZE보다 많이 남았을 때,
        if (temp_len >= PGSIZE) {
            f_info->page_read_bytes = PGSIZE;
            f_info->page_zero_bytes = 0;
            vm_alloc_page_with_initializer(VM_FILE, temp_addr, writable, lazy_load_segment, f_info);
            temp_offset += PGSIZE;
            temp_len -= PGSIZE;
            temp_addr += PGSIZE;
        }

        // PGSIZE보다 적게 남았을 때,
        else {
            f_info->page_read_bytes = temp_len;
            f_info->page_zero_bytes = PGSIZE - temp_len;
            vm_alloc_page_with_initializer(VM_FILE, temp_addr, writable, lazy_load_segment, f_info);
            break;
        }
    }
    return addr;
}

/* Do the munmap */
/* munmap 함수 구현 */
void do_munmap(void *addr) {
    struct thread* t = thread_current();
    struct supplemental_page_table* spt = &t->spt;

    if (addr == NULL || is_kernel_vaddr(addr)) {
        exit(-1);
    }
    
    // addr이 page_aligned 되어 있지 않을 때,
    if (pg_ofs (addr) != 0) {
        exit(-1);
    }

    // 찾았는데 없을 때,
    struct page* origin_page = spt_find_page(spt, addr);
    if (origin_page == NULL) {
        exit(-1);
    }

    // file 구하기

    /* do_mmap의 카운터. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
    /* addr에서 시작해서 해당 가상페이지가 addr을 시작으로 mmap한 페이지인지 확인*/
    /* 1. vm_get_type()을 써서 해당 페이지가 file 타입인지 확인
        2. file타입이면, file_page에 접근해서 fd 확인 후, addr로 구한 페이지의 file_page 확인해서 fd 확인 
    */

   // page -> uninit-> aux -> file -> fd 구함. addr -> page -> uninit -> aux -> file 구함. 둘이 비교

    struct file* origin_file = ((struct file_info*)origin_page->uninit.aux)->file;
    uint64_t va = (uint64_t)addr + PGSIZE;
    struct page* temp_page;

   while (true) {
        temp_page = spt_find_page(spt, va);    

        // 못 찾았으면 break.
        if (temp_page == NULL) {
            break;
        }

        // 찾았을 때,

        struct file* temp_file;
        // 타입이 vm_file이고, 같은 file에 대한 페이지면 dealloc
        if (page_get_type(temp_page) == VM_FILE) {
            temp_file = ((struct file_info*)temp_page->uninit.aux)->file;
            if (origin_file == temp_file) {
                vm_dealloc_page(temp_page);
                va += PGSIZE;
            }
            else {
                break;
            }
        } 
   }
   vm_dealloc_page(origin_page);
}
