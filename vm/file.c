/* file.c: Implementation of memory backed file object (mmaped object). */
/* memory backed file 개체 구현 파일file.c */

// clang-format off
#include "vm/vm.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include <hash.h> // SPT 해시테이블을 위해서 추가
#include "lib/round.h"
#include "threads/malloc.h"
#include "threads/mmu.h"

// #define VM
// clang-format on

static bool file_backed_swap_in(struct page *page, void *kva);
static bool file_backed_swap_out(struct page *page);
static void file_backed_destroy(struct page *page);
static bool mmap_load_segment(struct file *file, long ofs, uint8_t *upage, long read_bytes, uint32_t zero_bytes, bool writable);
bool check_munmap_va(void* addr);
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

    struct file_info* file_page_info = (struct file_info*)page->uninit.aux;
    page->file.file = file_page_info->file;
    page->file.offset = file_page_info->offset;
    page->file.read_bytes = file_page_info->page_read_bytes;
    page->file.zero_bytes = file_page_info->page_zero_bytes;
    page->file.writable = file_page_info->writable;
    page->file.init_mmaped_va = file_page_info->init_mapped_va;

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
    struct thread* t = thread_current();
    
    // write된거면
    if (pml4_is_dirty(t->pml4, page->va)) {
        // 지울 때 쓰기
        file_write_at(page->file.file, page->va, page->file.read_bytes, page->file.offset);
        
        // memcpy()
        /*
                // size_t read_bytes;
    //             size_t zero_bytes;
    //             off_t offset;
    //     */
       // write 정보


    }
}

/* Do the mmap */
/* mmap 함수 구현 */
/* fd로 열린 file을 offset부터 length만큼 읽어서 addr에 lazy하게 매핑하겠다. */
void *do_mmap(void *addr, long length, int writable, struct file *file, long offset) {

    /* 파일을 메모리에 매핑하는 함수. 유저의 VA, 바이트 크기, Write 가능여부, 파일 포인터, 그리고 Offset을 활용. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
    
    // 가상 주소 addr부터, addr + length까지 매핑됨
    // 매핑되는 값은 file의 offset지점부터 offset + length지점까지임
    // open할 때 어떻게 하는지 보기

    /*
        length를 0부터 시작해서 PGSIZE를 더해가며 page initialize 수행
    */

    /******기존에 사용되던 변수****/
    //struct file_info* f_info;
    // size_t temp_len = length; // len > 0 만족
    // size_t temp_offset = offset;
    /*************************/

    struct thread* t = thread_current();
    uint64_t temp_addr = (uint64_t)addr; // page_aligned 만족
    uint32_t pages_size = (uint32_t)ROUND_UP(length, PGSIZE);

    if (length == 0) {
        pages_size += PGSIZE;
    }

    // 끝 페이지가 kernel 영역 침범하는지 확인
    if (temp_addr + length > KERN_BASE) {
        return NULL;
    }

    // 일단 해당 파일을 연속된 가상페이지에 매핑할 수 있는지부터 확인
    for (temp_addr; temp_addr < (uint64_t)addr + (uint64_t)pages_size; temp_addr += PGSIZE) {
        // spt에 들어간게 없어야 함.
        if (spt_find_page(&t->spt, (void*)temp_addr)) {
            return NULL;
        }
    }

    // printf("주소: %p, 길이: %d\n", addr, length);
    // file, offset, va, read_bytes, zero_bytes
    if (!mmap_load_segment(file, offset, addr, length, pages_size - length, writable)) {
        return NULL;
    }

    return addr;

    /*******기존 코드*******/
    // 매핑할게 남아 있을 때,
    // while (temp_len > 0) {
    //     if (spt_find_page(&t->spt, temp_addr)) {
    //         // 매핑 되어있을 때, 지금까지 만들었던 페이지 구조체 삭제 및 NULL 반환
    //         for (char va = (char)addr; va < temp_addr; va += PGSIZE) {
    //             struct page* page = spt_find_page(&t->spt, va);
    //             spt_remove_page(&t->spt, page);
    //         }
    //         return NULL;
    //     }

    //     // 매핑 안 되어 있을 때,

    //     f_info = (struct file_info*)calloc(1, sizeof(struct file_info));
    //     f_info->file = file;
    //     f_info->offset = temp_offset;

    //     // PGSIZE보다 많이 남았을 때,
    //     if (temp_len >= PGSIZE) {
    //         f_info->page_read_bytes = PGSIZE;
    //         f_info->page_zero_bytes = 0;
    //         vm_alloc_page_with_initializer(VM_FILE, temp_addr, writable, lazy_load_segment, f_info);
    //         temp_offset += PGSIZE;
    //         temp_len -= PGSIZE;
    //         temp_addr += PGSIZE;
    //     }

    //     // PGSIZE보다 적게 남았을 때,
    //     else {
    //         f_info->page_read_bytes = temp_len;
    //         f_info->page_zero_bytes = PGSIZE - temp_len;
    //         vm_alloc_page_with_initializer(VM_FILE, temp_addr, writable, lazy_load_segment, f_info);
    //         break;
    //     }
    // }
    /*******기존 코드*******/

    return addr;
}

/* Do the munmap */
/* munmap 함수 구현 */
void do_munmap(void *addr) {
    // addr 유효성 체크
    if (!check_munmap_va(addr)) {
        exit(-1);
    }
    // 여기 지나면 이제 유효한 가상주소 들어옴
    
    struct thread* t = thread_current();
    struct supplemental_page_table* spt = &t->spt;

    struct page* origin_page = spt_find_page(spt, addr);

    char* temp_addr = (char*)addr;
    struct page* temp_page;
    struct file_info* temp_file_info;
    struct file* origin_file;

    // file 저장 코드
    if (origin_page->frame == NULL) {
        temp_file_info = (struct file_info*)origin_page->uninit.aux;
        origin_file = temp_file_info->file;
    }
    else {
        origin_file = origin_page->file.file;
    }

    // page free 코드
    while (true) {
        // 일단 temp_addr 유효성 확인, 커널영역인지만 확인
        if (is_kernel_vaddr(temp_addr)) {
           break;
        }

        temp_page = spt_find_page(spt, (void*)temp_addr);

        // 없으면 끝
        if (temp_page == NULL) {
            break;
        }

        // 로딩 끝난거면, 
        if (temp_page->frame != NULL) {
            // 파일타입이고, addr에 매핑된 파일과 같은 파일이면 
            if (page_get_type(temp_page) == VM_FILE && (uint64_t)temp_page->file.init_mmaped_va == (uint64_t)addr) {
                spt_remove_page(&t->spt, temp_page); // file_destroy() 함수에서 dirty 체크 후 writeback 판단
            }
            else {
                break;
            }
        }

        // 아직 로딩은 안된거면
        else {
            temp_file_info = (struct file_info*)temp_page->uninit.aux;
            // 파일타입이고, addr에 매핑된 파일과 같은 파일이면 
            if (temp_page->uninit.type == VM_FILE && (uint64_t)temp_file_info->init_mapped_va == (uint64_t)addr) {
                spt_remove_page(&t->spt, temp_page);
            }
            else {
                break;
            }
        }
        // 다음 페이지 확인
        temp_addr += PGSIZE;
    }

    free(origin_file);

    // spt_remove_page(spt, temp_page)를 하면,
        // spt에서 page 구조체 빼버림
        // 페이지 타입별 destroy 함수 호출
            // uninit인 경우 -> free(page->uninit.aux);
            // file인 경우 -> dirty bit 기반 파일 변경사항 저장
        // page 구조체 free


    // 로딩 됐나 안됐나 판단
        // 로딩 됐을 시:
            // 파일 타입일 시: file 구조체에 적은 mmap 가상주소와 인자로 들어온 addr 비교
                // 다를 때: No return
                // 같을 때: for문 돌면서 spt_remove(page) 수행
            // 파일 타입 아닐 시: No return
        // 로딩 안됐을 시:
            // uninit_page->


    /* do_mmap의 카운터. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
    /* addr에서 시작해서 해당 가상페이지가 addr을 시작으로 mmap한 페이지인지 확인*/
    /* 1. vm_get_type()을 써서 해당 페이지가 file 타입인지 확인
        2. file타입이면, file_page에 접근해서 fd 확인 후, addr로 구한 페이지의 file_page 확인해서 fd 확인 
    */

   // page -> uninit-> aux -> file -> fd 구함. addr -> page -> uninit -> aux -> file 구함. 둘이 비교

    /*
        munmap 구현방법
        munmap: 인자로 받은 addr에 대해 해당 파일과 매핑되어 있는 모든 페이지 할당 해제
    */

   // 해당 addr이 

   /****************** 기존 코드 ****************/
//     struct file* origin_file = ((struct file_info*)origin_page->uninit.aux)->file;
//     uint64_t va = (uint64_t)addr + PGSIZE;
//     struct page* temp_page;

//    while (true) {
//         temp_page = spt_find_page(spt, va);    

//         // 못 찾았으면 break.
//         if (temp_page == NULL) {
//             break;
//         }

//         // 찾았을 때,

//         struct file* temp_file;
//         // 타입이 vm_file이고, 같은 file에 대한 페이지면 dealloc
//         if (page_get_type(temp_page) == VM_FILE) {
//             temp_file = ((struct file_info*)temp_page->uninit.aux)->file;
//             if (origin_file == temp_file) {
//                 vm_dealloc_page(temp_page);
//                 va += PGSIZE;
//             }
//             else {
//                 break;
//             }
//         } 
//    }
//    vm_dealloc_page(origin_page);
   /****************** 기존 코드 ****************/
}

bool check_munmap_va(void* addr) {
    if (addr == NULL || is_kernel_vaddr(addr)) {
        return false;
    }
    
    // addr이 page_aligned 되어 있지 않을 때,
    if (pg_ofs (addr) != 0) {
        return false;
    }
    struct thread* t = thread_current();

    // 찾았는데 없을 때,
    struct page* origin_page = spt_find_page(&t->spt, addr);
    if (origin_page == NULL) {
        return false;
    }
    struct file_info* f_info;
    // origin_page 로딩X -> uninit_page->aux로 판단
    if (origin_page->frame == NULL) {
        if (origin_page->uninit.type == VM_FILE) {
            f_info = (struct file_info*)origin_page->uninit.aux;

            // 인자로 들어온 addr과 mmap할 때 저장해놓았던 init_mapped_va랑 비교해서 다르면 무효
            if ((uint64_t)f_info->init_mapped_va != (uint64_t)addr) {
                return false;
            }
            // 같을 때,
            else {
                // page에 저장된 va와 같다면 이 페이지가 시작페이지임
                if ((uint64_t)origin_page->va != (uint64_t)f_info->init_mapped_va) {
                    return false;
                }
            }
        }
        else {
            return false;
        }
    }

    // origin_page 로딩O -> page.file로 판단
    else {
        // 파일 타입일 때,
        if (page_get_type(origin_page) == VM_FILE) {
            // 다른 파일의 mmap이면 exit(-1)
            if ((uint64_t)origin_page->file.init_mmaped_va != (uint64_t)addr) {
                return false;
            }
            // 같은 파일의 mmap일 때,
            else {
                // page에 저장된 va와 init_mapped_va가 같다면 이 페이지가 시작페이지임
                if ((uint64_t)origin_page->va != (uint64_t)origin_page->file.init_mmaped_va) {
                    return false;
                }
            }
        }
        // 다른 파일 타입일 때,
        else {
            return false;
        }
    }

    return true;
}

static bool mmap_load_segment(struct file *file, long ofs, uint8_t *upage, long read_bytes, uint32_t zero_bytes, bool writable) {
    ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
    ASSERT(pg_ofs(upage) == 0);
    ASSERT(ofs % PGSIZE == 0);
 
    // printf("주소: %p, read_bytes: %d, zero_bytes: %d\n", upage, read_bytes, zero_bytes);
    while (read_bytes > 0 || zero_bytes > 0) {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        struct file_info* f_info = (struct file_info*)calloc(1, sizeof(struct file_info));
        
        f_info->file = file;
        f_info->writable = writable;
        f_info->offset = ofs;
        f_info->init_mapped_va = upage;
        f_info->page_read_bytes = page_read_bytes;
        f_info->page_zero_bytes = page_zero_bytes;
        
        if (!vm_alloc_page_with_initializer(VM_FILE, upage, writable, lazy_load_segment, f_info))
            return false;

        /* Advance. */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;

        ofs += page_read_bytes;
    }
    return true;
}