/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

// clang-format off
#include "vm/vm.h"
#include "bitmap.h" // Swap Table
#include "devices/disk.h" // Swap Table
#include <hash.h> // SPT 해시테이블을 위해서 추가

// #define VM
// clang-format on

/* DO NOT MODIFY BELOW LINE */
// static struct disk *swap_disk;
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
    /* Anonymous Page의 데이터를 채워주는 초기화 함수 */
    /* 그냥 모아서 보기 위해서 vm_init() 에서 호출됨; vm_init에서 대부분 처리해버림 (vm.h externs) */
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {

    page->operations = &anon_ops;

    struct anon_page *anon_page = &page->anon;
    anon_page->swap_disk_sector_start = -1;

    return true;
}

/* Swap in the page by read contents from the swap disk. */
/* Swapdisk에서 페이지를 불러와서 프레임에 넣는 함수 */
static bool anon_swap_in(struct page *page, void *kva) {

    /* swap_in하기 전에 frame을 확보하고 kva값을 제공함 */
    struct anon_page *anon_page = &page->anon;
    if (anon_page->swap_disk_sector_start == -1) {
        return false; // -1로 처음 초기화 되었으니, swap_out된적이 없다는 뜻
    }

    /* Disk에서 불러와서 frame->kva에 저장하고, page 값도 업데이트 */
    disk_read(swap_disk, (disk_sector_t)anon_page->swap_disk_sector_start, kva);
    // page->frame->page = page; // swap_in을 부르기 전에 "page = frame"을 이미 했음

    /* 해당 swap_disk_sector_start를 false로 돌리기 (해당 위치에 데이터가 없다고 표기) */
    bitmap_set_multiple(swap_bitmap, anon_page->swap_disk_sector_start, 8, false); // 해당 비트맵을 false로 비우기 (512 x 8 = 4096 PGSIZE)

    return true;
}

/* Swap out the page by writing contents to the swap disk. */
/* Swapdisk에 페이지를 넣는 함수 */
static bool anon_swap_out(struct page *page) {

    struct anon_page *anon_page = &page->anon;

    /* swap_out은 디스크에 저장하는 과정으로, 먼저 bitmap으로 빈곳을 조회 ; 시작위치를 idx로 반환받아 저장 */
    size_t idx = bitmap_scan_and_flip(swap_bitmap, 0, 8, false); // false인 비트맵을 찾아서 true로 바꾸기
    if (idx == BITMAP_ERROR) {
        return false;
    }

    /* 해당 위치에 디스크 저장 및 anon_page->swap_disk_sector_start 업데이트 */
    disk_write(swap_disk, (disk_sector_t)idx, (void *)page->frame->kva);
    anon_page->swap_disk_sector_start = idx;

    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
    struct thread *curr = thread_current();
    struct hash *spt_table = &curr->spt.spt_hash_table;
    struct anon_page *anon_page = &page->anon;

    pml4_clear_page((uint64_t *)curr->pml4, (void *)page->va);
    // free(page->uninit.aux);
    // free(page->frame->kva);
    // free(page->frame);
    // hash_delete(&spt_table, &page->spt_hash_elem);
}
