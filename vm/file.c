/* file.c: Implementation of memory backed file object (mmaped object). */

// clang-format off
#include "vm/vm.h"
#include "threads/malloc.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "bitmap.h" // Swap Table
#include "devices/disk.h" // Swap Table
#include <hash.h> // SPT 해시테이블을 위해서 추가
#include <stdlib.h>

// #define VM
// clang-format on

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);

/* Prototype for mmap */
static bool load_segment_mmap(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);
static bool lazy_load_mmap(struct page *page, void *aux);

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
    /* 딱히 여기서 할게 보이진 않음 (다른곳에서 다 처리) */
}

/* Initialize the file backed page */
/* File-backed 페이지를 초기화 하는 전용 함수.
   vm_alloc_page_with_initializer()에서 사용될 함수로 보임.
   해당 페이지의 operations 멤버를 file_ops로 정해주는 등. */
bool file_backed_initializer(struct page *page, enum vm_type type, void *kva) {

    /* 원래 포함되어있던 operation으로, page의 function pointer 변경 */
    page->operations = &file_ops;
    page->initialized_by_file_initializer = true;

    /* lazy_load_aux에 담긴 값들을 file_page의 정보로 백업 */
    struct file_page *file_page = &page->file;
    struct lazy_load_aux *aux = (struct lazy_load_aux *)page->uninit.aux;
    file_page->origin_file = aux->file;
    file_page->offset = aux->offset;
    file_page->read_bytes = aux->read_bytes;
    file_page->zero_bytes = aux->zero_bytes;
    file_page->writable = aux->writable;
    file_page->first_page_va = aux->first_page_va;
    file_page->mmap_page_count = aux->mmap_page_count;

    return true;
}

/* Swap in the page by read contents from the file. */
/* 파일에서 데이터를 읽어온 뒤 해당 해당 페이지를 DRAM에 로딩하는 함수. */
static bool file_backed_swap_in(struct page *page, void *kva) {

    struct file_page *file_page = &page->file;
    struct file *origin_file = file_page->origin_file;
    off_t offset = file_page->offset;
    uint32_t read_bytes = file_page->read_bytes;
    uint32_t zero_bytes = file_page->zero_bytes;
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0)

    /* Swap-out 당시 저장된 파일에서 kva로 값을 복사 */
    int bytes_read = file_read_at(origin_file, kva, read_bytes, offset);
    if (bytes_read != read_bytes) {
        memset((uint8_t *)kva, 0, PGSIZE);
        return false;
    }

    /* 만일 페이지에 zero bytes가 있었다면 관련 값도 처리 */
    if (zero_bytes > 0) {
        memset((uint8_t *)kva + read_bytes, 0, zero_bytes);
    }

    return true;
}

/* Swap out the page by writeback contents to the file. */
/* DRAM에서 해당 페이지를 제거한 뒤 디스크에 변경사항을 저장하는 함수. */
static bool file_backed_swap_out(struct page *page) {

    ASSERT(page->frame != NULL);

    struct thread *curr = thread_current();
    struct file_page *file_page = &page->file;
    struct file *origin_file = file_page->origin_file;

    if (pml4_is_dirty(curr->pml4, page->va)) {

        /* 파일에다가 현재 버퍼 (va)의 내용을 덮어쓰기 (페이지 단위) */
        off_t bytes_written = file_write_at(origin_file, page->frame->kva, page->file.read_bytes + page->file.zero_bytes, page->file.offset);
        ASSERT(bytes_written == PGSIZE);

        // /* 수동으로 dirty bit 원래대로 돌리기 (Write하면 MMU가 dirty 체크를 해주지만, 이 경우는 안해줌) */
        // pml4_set_dirty(curr->pml4, page->va, 0);
    }

    /* KVA 영역을 0으로 채우기 */
    memset(page->frame->kva, 0, PGSIZE);

    /* 페이지테이블 연결내역 제거 */
    pml4_clear_page(curr->pml4, page->va);

    return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void file_backed_destroy(struct page *page) {

    /* File-backed 페이지와 관련된 리소스를 전부 Free해주는 함수. 파일을 닫는 등의 행위도 일어나야 함. */
    /* process_exit -> process_cleanup -> spt_kill -> hash_destroy + destroy_page -> vm_dealloc_page -> file_backed_destroy et al */

    struct thread *curr = thread_current();
    struct hash *spt_table = &curr->spt.spt_hash_table;
    struct file_page *file_page = &page->file;

    /* 해당 페이지를 삭제하게 된다면, dirty할 경우 원래 파일에 저장 ; MMU가 알아서 해준다 함 */
    if (pml4_is_dirty(curr->pml4, page->va)) {
        file_write_at(file_page->origin_file, page->va, file_page->read_bytes, file_page->offset);
    } // file_close는 process_exit에서 해줘야 할 것 같음 (fd 접근 어려움)

    pml4_clear_page((uint64_t *)curr->pml4, (void *)page->va);

    if (page->frame != NULL) {
        struct frame_list_elem *e = &page->frame->frame_list_elem;
        list_remove(e);

        void *kva = page->frame->kva;
        page->frame->page = NULL;
        page->frame = NULL;
        palloc_free_page(kva);
    }
    // free(page->uninit.aux);
    // free(page->frame->kva);
    // free(page->frame);
    // hash_delete(&spt_table, &page->spt_hash_elem);
}

/* 파일을 메모리에 매핑하는 함수. 유저의 VA, 바이트 크기, Write 가능여부, 파일 포인터, 그리고 Offset을 활용. */
void *do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset) {

    /* 주소가 Page-align 되어있지 않거나, 매핑하려는 바이트 수가 0과 같거나 작다면 */
    if (pg_round_down(addr) != addr || length <= 0) {
        return NULL;
    }

    /* 매핑하려는 범위가 다른 함수와 중복되지 않도록 검증  */
    struct thread *curr = thread_current();
    uint64_t *target_page = (uint64_t *)addr;
    uint64_t *target_last_page = (uint64_t *)pg_round_down((uint64_t *)addr + length);
    for (target_page; target_page <= target_last_page; target_page += PGSIZE) { // 페이지 단위로 마지막 페이지까지 반복
        if (spt_find_page(&curr->spt, target_page)) {
            return NULL;
        }
    }

    /* 목표 파일을 새로 열어서 기존의 열린 파일들과 분리 (fd는 별도로 안주고 일단 진행해봄) */
    struct file *reopened_file = file_duplicate(file);

    /* Read & Zero-bytes 세팅*/
    uint32_t read_bytes;
    uint32_t zero_bytes;
    uint32_t file_len = file_length(reopened_file);

    read_bytes = length > file_len ? file_len : length;
    zero_bytes = length - read_bytes;

    uint32_t excess = (read_bytes + zero_bytes) % PGSIZE;
    if (excess) {
        zero_bytes += PGSIZE - excess;
    }

    /* 새로 연 파일을 Caller 스레드의 VA에 매핑 */
    if (!load_segment_mmap(reopened_file, offset, addr, read_bytes, zero_bytes, writable)) { // load_segment에 인자를 하나 추가, mmap여부 표시
        return NULL;
    }

    /* 매핑에 성공했으니 */
    return addr;
}

// if (length > file_length(reopened_file)) {
//     /* 읽으려는 크기가 파일보다 크다면 */
//     read_bytes = file_length(reopened_file);
//     zero_bytes = length - file_length(reopened_file);
// } else {
//     /* 읽으려는 크기가 파일보다 작다면 */
//     read_bytes = length;
//     zero_bytes = 0;
// }
// if (length % PGSIZE == 0) {
//     /* zero-bytes 추가 세팅 ; 이미 length가 page-size align이라면 건드릴게 없음 */
//     zero_bytes += 0;
// } else {
//     /* zero-bytes 추가 세팅 ; page-size alignment를 위한 추가작업 */
//     zero_bytes += PGSIZE - (length % PGSIZE); // zero로 채워야하는 값은 마지막 페이지에만 한정 ; 잔여 length에서 pgsize로 나눈 나머지 값
// }

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

    /* 해당 페이지와 관련된 파일을 찾아서, */
    struct file *mmap_file = NULL;
    size_t mmap_count;

    struct file_page *file_page = &target_page->file;
    struct lazy_load_aux *aux = target_page->uninit.aux;
    bool status = target_page->initialized_by_file_initializer; // anon/file initializer로 초기화될 때 true로 변경

    /* file_page가 있다면 로딩된 상태 */
    if (status) {
        mmap_file = file_page->origin_file;
        mmap_count = file_page->mmap_page_count;
    } else {
        mmap_file = aux->file;
        mmap_count = aux->mmap_page_count;
    }

    if (mmap_file == NULL || mmap_count <= 0) {
        printf("mmap_file not found during do_munmap\n");
        exit(-1);
    }

    /* 반복문으로 파일과 관련된 페이지들을 전부 제거 */
    void *temp_addr = addr;
    for (int i = 0; i < mmap_count; i++) {

        /* 만일 각 페이지가 dirty로 태그되었다면 (vm_alloc_page_with_initializer() 참고) */
        if (pml4_is_dirty(curr->pml4, temp_addr)) {

            /* 파일에다가 현재 버퍼 (va)의 내용을 덮어쓰기 (페이지 단위) */
            file_write_at(mmap_file, temp_addr, target_page->file.read_bytes, target_page->file.offset);
        }

        /* 작업이 끝났으니 SPT와 PTE 제거, 프레임에 있었다면 evict */
        spt_remove_page(spt, target_page); // 여기서 file_backed_destroy까지 가면 Frame도 비워줘야 함

        /* 다음 페이지로 이동 */
        temp_addr += PGSIZE;
        target_page = spt_find_page(spt, temp_addr);

        /* 커널로 진입했거나, 연속된 페이지가 없다면 반복문 해제 */
        if (is_kernel_vaddr(temp_addr) || target_page == NULL) {
            break;
        }
    }

    /* 최종적으로 파일 닫아주기 */
    if (mmap_file) {
        file_close(mmap_file);
    }
}

static bool lazy_load_mmap(struct page *page, void *aux) {

    struct lazy_load_aux *info = (struct lazy_load_aux *)aux;

    if (!info)
        return false;

    /* 주어진 데이터를 기반으로 파일을 찾기 (현재 핀토스는 FILESYS를 기본 모드로 사용 ; 따라서 디렉토리 하나라 간단함) */
    file_seek(info->file, info->offset);

    /* 로딩 예정인 페이지의 프레임을 정의하고, */
    uint8_t *frame_addr = page->frame->kva;
    if (frame_addr == NULL) {
        return false;
    }

    /* 정의한 프레임에다 파일을 불러오면 됨 */
    if (file_read(info->file, frame_addr, info->read_bytes) != (int)info->read_bytes) {
        return false; // file_read는 읽어온 byte를 반환하기에, lazy_load_aux로 전달된 양과 비교
    }

    /* 불러온 영역을 제외한 나머지는 0으로 채워야 함 */
    memset(frame_addr + info->read_bytes, 0, info->zero_bytes);

    /* 성공 */
    aux = NULL;
    free(aux);
    return true;
}

static bool load_segment_mmap(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable) {
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);

    /* aux에 넣어줄 최초의 upage값 백업 */
    uint64_t first_mapped_origin = upage;

    /* aux에 넣어줄 mmap 페이지 수 계산 */
    size_t page_count = (read_bytes + zero_bytes) / PGSIZE;

    while (read_bytes > 0 || zero_bytes > 0) {

        /* FILE에서 PAGE_READ_BYTES를 읽고, 마지막 PAGE_ZERO_BYTES 만큼을 ZERO로 채움 */
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        /* vm.h에 만든 lazy_load_info 구조를 사용해서 aux로 전달해야 함 ; 일단 메모리 공간 확보 */
        struct lazy_load_aux *info = (struct lazy_load_aux *)calloc(1, sizeof(struct lazy_load_aux));
        if (!info) {
            return false;
        }

        /* malloc으로 공간을 확보했으니 load_segment()로 전달받은 데이터로 채우기 */
        info->file = file;
        info->offset = ofs;
        info->read_bytes = page_read_bytes;
        info->zero_bytes = page_zero_bytes;
        info->writable = writable;
        info->first_page_va = first_mapped_origin; // mmap의 시작점인 첫 페이지를 기록
        info->mmap_page_count = page_count;

        /* 만들어둔 데이터 구조체를 사용해서 페이지 초기화 작업 수행 */
        if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_load_mmap, info)) { // WHY NOT VM_FILE
            free(info);
            return false;
        }

        /* 인자로 주어졌던 값들을 업데이트 */
        ofs += page_read_bytes; // 이건 직접 추가, 나머지는 이미 있었음
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
    }
    return true;
}