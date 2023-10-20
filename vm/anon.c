/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

// clang-format off
#include "vm/vm.h"
#include "devices/disk.h"
#include <hash.h> // SPT 해시테이블을 위해서 추가
#include "threads/mmu.h"
#include "lib/kernel/bitmap.h"
#include "threads/vaddr.h"
// #define VM
// clang-format on
/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in(struct page *page, void *kva);
static bool anon_swap_out(struct page *page);
static void anon_destroy(struct page *page);

struct swap_table {
    struct bitmap* bm;
};

struct lock swap_lock;

struct swap_table swap_t;
/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
    .swap_in = anon_swap_in,
    .swap_out = anon_swap_out,
    .destroy = anon_destroy,
    .type = VM_ANON,
};

#define NEED_SECTORS ((PGSIZE) / (DISK_SECTOR_SIZE))

/* Initialize the data for anonymous pages */
void vm_anon_init(void) {

    /* Anonymous Page의 데이터를 채워주는 초기화 함수 ; vm_init()에서 호출됨 */

    /* TODO: Set up the swap_disk. @@@@@@@@@@ */
    swap_disk = disk_get(1, 1);
    lock_init(&swap_lock);
    // size_t usable_pages_cnt = disk_size(swap_disk) / NEED_SECTORS;
    // printf("페이지 몇개까지 저장 가능?: %d\n", usable_pages_cnt); // swap-anon: 7560개
    swap_t.bm = bitmap_create(disk_size(swap_disk));
}

/* Initialize the file mapping */
bool anon_initializer(struct page *page, enum vm_type type, void *kva) {
    /* Set up the handler */
    page->operations = &anon_ops;

    struct anon_page *anon_page = &page->anon;
    anon_page->bit_idx = 0;
    return true;
}

int cnts = 0;
/* Swap in the page by read contents from the swap disk. */
static bool anon_swap_in(struct page *page, void *kva) {
    // printf("스왑인 몇번: %d\n", cnts++);
    struct anon_page *anon_page = &page->anon;
    // swap_in은 페이지와 프레임을 다 연결시킨 후 테이블 매핑까지 하고나서, 로딩하려고할 때 호출됨
    // 적재시켜주면 됨

    // 1. anon_page->bit_cnt를 통해 스왑 디스크 접근
    // 2. disk_read() 해서 페이지 데이터 읽어오면서 va에 적기

    char* temp_addr = (char*)kva;
    lock_acquire(&swap_lock);
    for (int i = 0; i < NEED_SECTORS; i++) {
        disk_read(swap_disk, anon_page->bit_idx + i, temp_addr);
        temp_addr += DISK_SECTOR_SIZE;
    }
    // 3. 비트맵 및 디스크 초기화
    if (bitmap_scan_and_flip(swap_t.bm, anon_page->bit_idx, NEED_SECTORS, true) == BITMAP_ERROR) {
        ASSERT(false && "anon swap in -> bitmap_scan_and_flip 하다 터짐");
        return false;
    };
    lock_release(&swap_lock);
    anon_page->bit_idx = 0;

    return true;
    }

/* Swap out the page by writing contents to the swap disk. */
static bool anon_swap_out(struct page *page) {
    // printf("아논 스왑아웃 되는중!!\n");
    struct anon_page *anon_page = &page->anon;
    struct bitmap* bm = swap_t.bm;
    char* temp_addr = (char*)page->frame->kva;

    //  1. 해당 페이지에 저장된 데이터 크기(1페이지)를 기준으로, 몇 개의 섹터가 필요한 지, 1섹터 = 512B, 
    
    //  1섹터 = 1비트 매핑
    lock_acquire(&swap_lock);
    size_t idx = bitmap_scan_and_flip(bm, 0, NEED_SECTORS, false);
    // printf("현재 idx: %d\n", idx);
    //  2-1. 실패시 false 리턴
    if (idx == BITMAP_ERROR) {
        ASSERT(idx != BITMAP_ERROR && "비트맵 매핑 실패");
        return false;
    }

    // 3. 디스크에 쓰기
    // 인자: 스왑디스크, 섹터넘버, 저장할 데이터 시작주소
    for (size_t i = 0; i < NEED_SECTORS; i++) {
        disk_write(swap_disk, idx + i, temp_addr);
        temp_addr += DISK_SECTOR_SIZE;
    }

    // 이제 page는 프레임 할당 전 상태로 돌아가야 함.
    // 페이지-프레임 연결 끊기
    page->frame->page = NULL;
    page->frame = NULL;

    // 2. 매핑 끊기
    pml4_clear_page(thread_current()->pml4, page->va);
    
    // anon_page에 bit_idx 저장
    anon_page->bit_idx = idx;
    lock_release(&swap_lock);
    return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void anon_destroy(struct page *page) {
    struct anon_page *anon_page = &page->anon;
    if (page->frame != NULL ){
        pml4_clear_page(thread_current()->pml4, page->va);
        palloc_free_page(page->frame->kva);
        // 
        free(page->frame);
    }
    // hash_delete(&thread_current()->spt, &page->spt_hash_elem);
}
// 