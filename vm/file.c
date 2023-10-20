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
#include "lib/string.h"

// #define VM
// clang-format on
bool file_load_segment(struct page* page, void* aux);
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
    page->file.page_cnts = file_page_info->page_cnts;

    return true;
}

/* Swap in the page by read contents from the file. */
/* 파일을 swap in 시켜줌 */
static bool file_backed_swap_in(struct page *page, void *kva) {

    /* 파일에서 데이터를 읽어온 뒤 해당 해당 페이지를 DRAM에 로딩하는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;

    // free(aux) 안해주기 위해 함수 만듬
    if (!file_load_segment(page, file_page)) {
        return false;
    }
    return true;
}

/* Swap out the page by writeback contents to the file. */
/* 파일을 swap out함 */
static int cnt = 0;
static bool file_backed_swap_out(struct page *page) {
    /* DRAM에서 해당 페이지를 제거한 뒤 디스크에 변경사항을 저장하는 함수. */
    // printf("파일백드 destroy로 옴?: %d\n", cnt++);
    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
    struct thread* t = thread_current();
    struct file_page *file_page UNUSED = &page->file;
    // dirty 체크 후, 써져있으면 파일에 저장

        // write된거면
    if (pml4_is_dirty(t->pml4, page->va)) {
        pml4_set_dirty(t->pml4, page->va, 0);
        // 지울 때 쓰기
        if (file_write_at(page->file.file, page->va, page->file.read_bytes, page->file.offset) != page->file.read_bytes) {
            printf("쓰기 실패함?\n");
            return false;
        }
    }

    // 스왑 테이블에 페이지 구조체 넣기 -> file은 스왑테이블 안씀.

    // 해당 물리메모리 영역 초기화
    memset(page->frame->kva, 0, PGSIZE);
    page->frame->page = NULL;
    page->frame = NULL;


    // 페이지테이블 연결 제거(이후, 다시 va에 접근시 page fault 발생 -> 해당 va와 연결된 page 구조체가 일단 스왑 테이블에 있나 확인 -> 없으면 기존로직 수행)
    pml4_clear_page(t->pml4, page->va);
    return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
/* 파일을 destory함 */
static void file_backed_destroy(struct page *page) {

    /* File-backed 페이지와 관련된 리소스를 전부 Free해주는 함수. 파일을 닫는 등의 행위도 일어나야 함. */
    /* 열었던 file만 닫아주고, file 변경사항은 디스크에 write 해주기 */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    struct file_page *file_page UNUSED = &page->file;
    struct thread* t = thread_current();

    // 파일 닫아줘야 함.
    // page 들어오면, 부모페이지 찾아서 다 써주기 + va = NULL로 바꿔주기 + 파일 삭제
    // 이후 클리어 
    
    file_page->file->file_backed_cnts++;
    // 스왑아웃 안된거만 처리!!
    if (page->frame != NULL) {
        // write된거면
        if (pml4_is_dirty(t->pml4, page->va)) {
            // 지울 때 쓰기
            file_write_at(page->file.file, page->va, page->file.read_bytes, page->file.offset);
        }
    }

    if (page->file.page_cnts == file_page->file->file_backed_cnts) {
        file_close(file_page->file);
    }

    if (page->frame != NULL) {
        pml4_clear_page(t->pml4, page->va);
        palloc_free_page(page->frame->kva);
        // hash_delete(&thread_current()->spt, &page->spt_hash_elem);
        free(page->frame);
    }
}

/* Do the mmap */
/* mmap 함수 구현 */
/* fd로 열린 file을 offset부터 length만큼 읽어서 addr에 lazy하게 매핑하겠다. */

    // if (origin_file) {
    //     lock_acquire(&t->mmap_lock);
    //     struct file* new_file = file_reopen(origin_file);
    //     off_t file_len = file_length(new_file);
    //     if (file_len != 0 && length == 0) {
    //         file_close(new_file);
    //         lock_release(&t->mmap_lock);
    //         return NULL;
    //     }
    //     else {
    //         va = do_mmap(addr, length, writable, new_file, offset);
    //         lock_release(&t->mmap_lock);
    //     }
    // }

void *do_mmap(void *addr, long length, int writable, struct file *file, long offset) {
    struct thread* t = thread_current();
    uint64_t temp_addr = (uint64_t)addr; // page_aligned 만족


    // file 만듬
    struct file* new_file = file_reopen(file);

    length = ROUND_UP(length, PGSIZE); // length: 매핑영역 크기(페이지단위)

    // 끝 페이지가 kernel 영역 침범하는지 확인
    if (temp_addr + length > KERN_BASE) {
        return NULL;
    }


    // 일단 해당 파일을 연속된 가상페이지에 매핑할 수 있는지부터 확인
    for (temp_addr; temp_addr < (uint64_t)addr + length; temp_addr += PGSIZE) {
        // spt에 들어간게 없어야 함.
        if (spt_find_page(&t->spt, (void*)temp_addr)) {
            return NULL;
        }
    }

    off_t file_len = file_length(new_file);
    long read_bytes = length < file_len ? length : file_len;

    // file, offset, va, read_bytes, zero_bytes
    if (!mmap_load_segment(new_file, offset, addr, read_bytes, length - read_bytes, writable)) {
        return NULL;
    }

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

    // 아직 로드 안됨 or 스왑아웃됨!
    if (origin_page->frame == NULL) {
        if (origin_page->uninit.type == VM_FILE) {
            temp_file_info = (struct file_info*)origin_page->uninit.aux;
            origin_file = temp_file_info->file;
        }
        // 로딩됐다가 스왑아웃된 페이지!!
        else if (page_get_type(origin_page) == VM_FILE) {
            origin_file = origin_page->file.file;
        }
    }

    // 로드됨!!
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

        // 아직 로딩은 안된거 or 스왑아웃된 파일페이지!!
        else {
            if (temp_page->uninit.type == VM_FILE) {
                temp_file_info = (struct file_info*)temp_page->uninit.aux;
                // 파일타입이고, addr에 매핑된 파일과 같은 파일이면 
                if ((uint64_t)temp_file_info->init_mapped_va == (uint64_t)addr) {
                    spt_remove_page(&t->spt, temp_page);
                }
            }
            // 스왑아웃된 파일페이지!!
            else if (page_get_type(temp_page) == VM_FILE) {
                if ((uint64_t)temp_page->file.init_mmaped_va == (uint64_t)addr) {
                    spt_remove_page(&t->spt, temp_page);
                }
            }
            else {
                break;
            }
        }
        // 다음 페이지 확인
        temp_addr += PGSIZE;
    }
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
    // origin_page frame 없을 때,
    if (origin_page->frame == NULL) {
        // uninit_page->aux가 VMFILE이면 아직 로딩 안된 페이지!
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
        // 프레임 없는데 uninit.type이 VM_FILE도 아니다? -> 로딩됐다가 스왑아웃된 파일페이지인지 확인!
        else if (page_get_type(origin_page) == VM_FILE){
            return true;
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
 
    uint8_t* init_addr = upage;
    uint32_t pagesize = read_bytes + zero_bytes;
    // printf("주소: %p, read_bytes: %d, zero_bytes: %d\n", upage, read_bytes, zero_bytes);
    while (read_bytes > 0 || zero_bytes > 0) {
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;
        struct file_info* f_info = (struct file_info*)calloc(1, sizeof(struct file_info));
        
        f_info->file = file;
        f_info->writable = writable;
        f_info->offset = ofs;
        f_info->init_mapped_va = (void*)init_addr;
        f_info->page_read_bytes = page_read_bytes;
        f_info->page_zero_bytes = page_zero_bytes;
        f_info->page_cnts = pagesize / PGSIZE;
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

bool file_load_segment(struct page* page, void* aux) {
    uint8_t *kpage = page->frame->kva;
    struct file_info* f_info = (struct file_info*)aux;
    struct thread* t = thread_current();

    file_seek(f_info->file, f_info->offset);
    if (file_read(f_info->file, kpage, f_info->page_read_bytes) != (int)(f_info->page_read_bytes)) {
        spt_remove_page(&t->spt, page);
        ASSERT(false && "lazy_load 시 실패");
        return false;
    }
    if (f_info->page_zero_bytes > 0) {
        memset(kpage + f_info->page_read_bytes, 0, f_info->page_zero_bytes);
    }

    return true;
}