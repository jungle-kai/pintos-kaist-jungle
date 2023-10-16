/* file.c: Implementation of memory backed file object (mmaped object). */

// clang-format off
#include "vm/vm.h"
#include "userprog/process.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include <hash.h> // SPT 해시테이블을 위해서 추가
#include <stdlib.h>

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
void vm_file_init(void) {

    /* File 전용 페이지에 Data를 채워서 초기화 해주는 함수 */

    /* @@@@@@@@@@ Todo @@@@@@@@@@ */
}

/* Initialize the file backed page */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva) {

    /* File-backed 페이지를 초기화 하는 전용 함수.
       vm_alloc_page_with_initializer()에서 사용될 함수로 보임.
       해당 페이지의 operations 멤버를 file_ops로 정해주는 등. */

    page->operations = &file_ops;

    struct file_page *file_page = &page->file;
}

/* Swap in the page by read contents from the file. */
static bool file_backed_swap_in(struct page *page, void *kva) {

    /* 파일에서 데이터를 읽어온 뒤 해당 해당 페이지를 DRAM에 로딩하는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool file_backed_swap_out(struct page *page) {

    /* DRAM에서 해당 페이지를 제거한 뒤 디스크에 변경사항을 저장하는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void file_backed_destroy(struct page *page) {

    /* File-backed 페이지와 관련된 리소스를 전부 Free해주는 함수. 파일을 닫는 등의 행위도 일어나야 함. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;
}

/* 파일을 메모리에 매핑하는 함수. 유저의 VA, 바이트 크기, Write 가능여부, 파일 포인터, 그리고 Offset을 활용. */
void *do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset) {

    /* 주소가 Page-align 되어있지 않거나, 매핑하려는 바이트 수가 0과 같거나 작다면 */
    if (pg_round_down(addr) != addr || length <= 0) {
        return NULL;
    }

    /* 매핑하려는 범위가 다른 함수와 중복되지 않도록 검증  */
    struct thread *curr = thread_current();
    char *target_page = (char *)addr;
    char *target_last_page = (char *)pg_round_down((char *)addr + length);
    for (target_page; target_page <= target_last_page; target_page += PGSIZE) { // 페이지 단위로 마지막 페이지까지 반복
        if (spt_find_page(&curr->spt, target_page)) {
            return NULL;
        }
    }

    /* 목표 파일을 새로 열어서 기존의 열린 파일들과 분리 */
    struct file *reopened_file = file_reopen(file);

    /* 새로 연 파일을 Caller 스레드의 VA에 매핑 */
    uint32_t read_bytes = length;          // 읽어야 하는 양은 offset에서 부터 length만큼
    uint32_t zero_bytes = length % PGSIZE; // zero로 채워야하는 값은 length에서 pgsize로 나눈 나머지 값 (마지막 페이지)
    if (!load_segment(reopened_file, offset, (uint8_t)addr, (uint32_t)read_bytes, (uint32_t)zero_bytes, writable)) {
        return NULL;
    }

    /* 매핑에 성공했으니 */
    return addr;
}

/* 인자로 제공하는 주소는 파일로 매핑된 첫 페이지의 시작 주소여야 하며, 해당 페이지가 Dirty라면 파일에 다시 저장한 뒤 리소스를 프리하고 파일을 닫는 함수. */
void do_munmap(void *addr) {

    /* Valid 한 유저 영역의 주소가 아니라면, page-align이 안되어있다면 거절 */
    if (addr == NULL || is_kernel_vaddr(addr) || pg_round_down(addr) != addr) {
        exit(-1);
    }

    struct thread *curr = thread_current();
    struct supplemental_page_table *spt = &curr->spt;

    /* Unmap하려는 페이지가 해당 스레드 소유가 아니라면 거절 */
    struct page *target_page = spt_find_page(spt, addr);
    if (target_page == NULL) {
        exit(-1);
    }

    /* TODO: 해당 스레드가 mmap된 페이지인지 확인 후 파일을 찾아서, 해당 파일로 mmap된 전체 영역에 걸쳐서 반복문으로 save if dirty + release resources and deallocate frames */
    /* TODO: 해당 매핑과 관련된 파일을 닫기 */

    /* 해당 페이지와 관련된 파일을 찾아서, */
    struct file *mmap_file = target_page->file.origin_file;
    if (mmap_file == NULL) {
        exit(-1);
    }

    /* 반복문으로 파일과 관련된 페이지들을 전부 제거 */
    void *curr_addr = addr;
    while (true) {

        /* 만일 각 페이지가 dirty로 태그되었다면 (vm_alloc_page_with_initializer() 참고) */
        if (pml4_is_dirty(curr->pml4, curr_addr)) {

            /* 파일에다가 현재 버퍼 (va)의 내용을 덮어쓰기 (페이지 단위) */
            file_write_at(mmap_file, curr_addr, target_page->file.read_bytes, target_page->file.offset);
        }

        /* 작업이 끝났으니 SPT와 PTE 제거, 프레임에 있었다면 evict */
        if (target_page->frame) { // 임시코드 (fb_destroy에서 해주면 삭제 요망)
            free(target_page->frame);
        }
        spt_remove_page(spt, target_page); // 여기서 file_backed_destroy까지 가면 Frame도 비워줘야 함
        uint64_t *pte = pml4e_walk(curr->pml4, curr_addr, 0);
        if (pte) {
            palloc_free_page((void *)PTE_ADDR(pte));
        }

        /* 다음 페이지로 이동 */
        curr_addr += PGSIZE;
        target_page = spt_find_page(spt, curr_addr);

        /* 연속된 페이지가 없거나, 있어도 파일이 같지 않다면 반복문 해제 */
        if (target_page == NULL || target_page->file.origin_file != mmap_file) {
            break;
        }
    }

    /* 최종적으로 파일 닫아주기 */
    if (mmap_file) {
        file_close(mmap_file);
    }
}
