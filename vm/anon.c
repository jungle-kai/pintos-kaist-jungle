/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

// clang-format off
#include "vm/vm.h"
#include "devices/disk.h"
#include <hash.h> // SPT 해시테이블을 위해서 추가

// #define VM
// clang-format on

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void vm_anon_init(void) {

    /* Anonymous Page의 데이터를 채워주는 초기화 함수 ; vm_init()에서 호출됨 */

    /* TODO: Set up the swap_disk. @@@@@@@@@@ */
    swap_disk = NULL;
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {
    /* Set up the handler */
    page->operations = &anon_ops;

    struct anon_page *anon_page = &page->anon;
    return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) { struct anon_page *anon_page = &page->anon; return true; }

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) { struct anon_page *anon_page = &page->anon; }

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) { struct anon_page *anon_page = &page->anon; }
