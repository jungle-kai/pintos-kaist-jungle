/* file.c: Implementation of memory backed file object (mmaped object). */
/* memory backed file 개체 구현 파일file.c */

// clang-format off
#include "vm/vm.h"
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

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
/* mmap 함수 구현 */
void *do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset) {

    /* 파일을 메모리에 매핑하는 함수. 유저의 VA, 바이트 크기, Write 가능여부, 파일 포인터, 그리고 Offset을 활용. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
}

/* Do the munmap */
/* munmap 함수 구현 */
void do_munmap(void *addr) {

    /* do_mmap의 카운터. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
}
