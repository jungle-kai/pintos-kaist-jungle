/* vm.c: Generic interface for virtual memory objects. */

// clang-format off
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"
#include "userprog/process.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <hash.h> // SPT 해시테이블을 위해서 추가
#include <list.h> // list 관련 추가

// #define VM
// clang-format on

/* Helper Prototypes */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* SPT Hashtable Helper Prototypes */
static bool install_page(void *upage, void *kpage, bool writable);
void destroy_page(struct hash_elem *e, void *aux);

/* Global Tables (or lists) */
struct frame_table frame_table;
struct swap_table swap_table;

/* Global Synchronization Primitives */
struct lock page_table_lock;
struct lock frame_table_lock;
struct lock swap_table_lock;

////////////////////////////////////////////////////////////////////////////////
//////////////////////// Virtual Memory System Init ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* Initializes the virtual memory subsystem by invoking each subsystem's intialize codes. */
void vm_init(void) {

    /* VM 시스템 부팅용 함수 (anon 시스템과 file시스템을 포함해서 전부) */

    vm_anon_init();
    vm_file_init();

#ifdef EFILESYS /* For project 4 */
    pagecache_init();
#endif

    /* 테스트를 위한 Inspect Interrupt를 초기화 하는 함수 (건드리지 말 것) */
    register_inspect_intr();

    /* DO NOT MODIFY UPPER LINES ; 수정은 이 아래로부터만 진행 */

    /* Page-table Related */
    lock_init(&page_table_lock); // 5개 모두 Global Variable로 선언되어 Data/BSS에 속함 (Palloc/Malloc/Calloc 불필요)

    /* Frame-table Related */
    // lock_init(&frame_table_lock);
    list_init(&frame_table.frame_table_list);

    /* Swap-table Related */
    // lock_init(&swap_table_lock);
    // list_init(&swap_table.swap_table_list);
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////// Page Content Retrival (helpers) ////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* Get the type of the page. This function is useful if you want to know the
   type of the page after it will be initialized. This function is fully implemented now. */
enum vm_type page_get_type(struct page *page) {

    /* 이미 부여된 페이지의 종류를 반환하는 함수.
       초기화 이후의 Type를 반환하며, <<수정 대상 아님>> */

    int ty = VM_TYPE(page->operations->type);

    switch (ty) {
    case VM_UNINIT:
        return VM_TYPE(page->uninit.type);
    default:
        return ty;
    }
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Page Initializer Core /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* Create the pending page object with initializer. If you want to create a page,
   do not create it directly and make it through this function or `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {

    /* 핀토스에서 모든 페이지는 처음 생성될 때 이 함수로 초기화/생성 되어야 함.
       대안 함수인 vm_alloc_page()는 마크로 <vm_alloc_page(type, upage, writable)>로 이 함수를 부름.
       upage는 USERPAGE를 의미함 ; 커널이 아닌 유저 공간의 가상 주소로, 페이지의 시작 주소 */

    ASSERT(VM_TYPE(type) != VM_UNINIT)

    /* 현재 스레드의 SPT를 정의 (접근) */
    struct supplemental_page_table *spt = &thread_current()->spt;

    /* 해시는 페이지의 시작주소를 기준으로 하기 때문에 Round_down을 해서 접근해야 함 */
    upage = pg_round_down(upage);

    /* 접근하려는 페이지가 이미 주인이 있는지 (사용중인지) 확인 후에 진행 */
    if (spt_find_page(spt, upage) == NULL) {

        /* TODO: Create the page, fetch the initializer according to the VM type,
                 and then create "uninit" page struct by calling uninit_new. You
                 should modify the field after calling the u    ninit_new. */

        /* 먼저 빈 페이지 메모리 확보 */
        struct page *new_page = calloc(1, sizeof(struct page));
        if (new_page == NULL) {
            goto err; // 메모리 할당 실패
        }

        /* 새로운 페이지 내용 채우기 */
        switch (type) {
        case VM_ANON:
            uninit_new(new_page, upage, init, type, aux, anon_initializer);
            break;
        case VM_FILE:
            uninit_new(new_page, upage, init, type, aux, file_backed_initializer);
            break;
        default:
            uninit_new(new_page, upage, init, type, aux, NULL);
            break;
            // ASSERT(false); // 뭔가 잘못되었으니 테스트 종료
        }

        /* 초기화하는 페이지에 Writable 및 Page Type 저장 (이전의 함수들에서 해주지 않음) */
        new_page->writable = writable;
        new_page->PAGE_TYPE = type;

        /* 초기화된 페이지를 스레드의 SPT에 삽입 */
        if (!spt_insert_page(spt, new_page)) {
            free(new_page);
            goto err;
        }

        /* 모든게 성공했으니 */
        return true;
    }
err:
    return false;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////// Supplemental Page Table Operations ///////////////////////
////////////////////////////////////////////////////////////////////////////////

/* Find VA from spt and return page. On error, return NULL. */
/* Parameter로 제공된 Supplementary Page Table (SPT)에서 페이지의 가상주소를 찾고 반환하는 함수. */
struct page *spt_find_page(struct supplemental_page_table *spt, void *va) {

    /* 주어진 주소를 round_down 해서 페이지를 찾고, */
    struct page search_page; // 임시 페이지로, 포인터 아님
    search_page.va = pg_round_down(va);

    /* 페이지를 기반으로 해시테이블을 검색 */
    struct hash *hash_table = &spt->spt_hash_table;
    struct hash_elem *search_elem = &search_page.spt_hash_elem;
    struct hash_elem *found_elem = hash_find(hash_table, search_elem);
    if (found_elem == NULL) {
        return NULL;
    }

    /* 찾은 elem을 기반으로 페이지를 재대로 정의 */
    struct page *page_found = hash_entry(found_elem, struct page, spt_hash_elem);

    /* 찾았으니 반환 */
    return page_found;
}

/* Insert PAGE into spt with validation. */
/* 페이지의 멤버인 Supplementary Page Table (SPT)에 페이지를 등록하는 함수. */
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page) {

    bool succ = false;

    /* 주어진 페이지를 해싱해서 삽입 */
    struct hash_elem *result = hash_insert(&spt->spt_hash_table, &page->spt_hash_elem);

    /* hash_insert()는 재대로 삽입한다면 Null Pointer 반환 */
    if (result == NULL) {
        succ = true;
    }

    return succ;
}

/* 페이지의 SPT에서 특정 페이지를 제거하는 함수 */
void spt_remove_page(struct supplemental_page_table *spt, struct page *page) {

    /* hash_delete()는 삭제에 성공한다면 삽입한 elem을 그대로 반환 */
    struct hash_elem *result = hash_delete(&spt->spt_hash_table, &page->spt_hash_elem);

    /* 삭제에 성공했으면 페이지 반환*/
    vm_dealloc_page(page);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////// Page Fault Handler for VM ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* Get the struct frame, that will be evicted. */
static struct frame *vm_get_victim(void) {

    /* LRU 등 팀에서 정한 알고리즘을 활용, DRAM에서 쫒아낼 Present Upage를 선정하는 함수. */

    struct frame *victim = NULL;

    /* @@@@@@@@@@ TODO: The policy for eviction is up to you. @@@@@@@@@@*/

    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *vm_evict_frame(void) {

    /* vm_get_victim()으로 선정한 페이지를 DRAM에서 쫒아내는 함수. */

    struct frame *victim UNUSED = vm_get_victim();

    /* @@@@@@@@@@ TODO: swap out the victim and return the evicted frame. @@@@@@@@@@ */

    return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *vm_get_frame(void) {

    /* Frame Table을 탐색하고, 빈 Frame을 반환하는 함수.
       빈 Frame이 없다면 vm_evict_frame()으로 공간을 확보한 뒤에 반환. */

    struct frame *frame = (struct frame *)calloc(1, sizeof(struct frame));
    if (frame == NULL) {
        return NULL;
    }

    frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);
    if (frame->kva == NULL) {
        free(frame);
        frame = vm_evict_frame();
    }

    ASSERT(frame != NULL);
    ASSERT(frame->page == NULL);
    return frame;
}

/* Growing the stack. */
static void vm_stack_growth(void *addr UNUSED) {

    /* 스택의 크기를 키울 떄 사용하는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
}

/* Handle the fault on write_protected page */
static bool vm_handle_wp(struct page *page UNUSED) {

    /* Write_protect가 걸려있는 페이지에 접근하여 fault가 발생할 경우 Invoke 되는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
}

/* Page Fault 발생 시 처음 Invoke 되는 함수. 성공시 True 리턴.
   Fault 발생 사유에 따라서 어떤 조치가 필요한지 확인하고, 해당 액션을 통해서 페이지를 확보해오는 함수. */
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user, bool write, bool not_present) {

    /* Page Fault에서는 다음과 같이 이 함수를 부름:
       vm_try_handle_fault(f, fault_addr, user, write, not_present) */

    struct thread *curr = thread_current();
    struct supplemental_page_table *spt = &curr->spt;
    uint64_t *pml4 = curr->pml4;
    struct page *page = NULL;

    /* (1) Validate The Fault ; 접근하면 안되는 영역인지 확인 */
    if (addr == NULL || is_kernel_vaddr(addr)) {
        return false;
    }

    /* (2) Stack 관련이면 여기서 미리 처리 */
    if (addr <= f->rsp && addr >= f->rsp - (PGSIZE / 4)) {       // Fault 주소가 스택 페이지 바로 다음 페이지 범위 안에 있으면
        if (USER_STACK - (uint64_t)addr > STACK_RESERVED_SIZE) { // 만일 1MB를 넘어가면 중지
            return false;
        }
        addr = pg_round_down(addr);
        return vm_claim_page(addr); // 1MB를 넘지 않는다면 스택 연장
    }
    // if (addr == (void *)f->rsp) {
    //     return vm_claim_page(addr);
    // }

    /* (3) Locate page in thread's SPT ; 스레드가 보유한게 맞는지 확인 */
    addr = pg_round_down(addr);
    page = spt_find_page(spt, addr);
    if (!page) {
        return false;
    }

    /* (3) 만일 write operation인데 page write 권한이 없다면 */
    if (write && !page->writable) {
        return false;
    }

    /* (4) 페이지 프레임 확보 */
    if (page->frame == NULL) {

        /* Write 때문에 Fault가 생겼다면, 성공 표기 하기 전에 dirty로 표시 */
        // if (write) {
        //     pml4_set_dirty(pml4, page->va, true);
        // }

        /* Frame의 확보, 페이지와 연결, PTE 생성/삽입, 물리 메모리에 넣는 것까지 전부 vm_do_claim_page 에서 수행 */
        return vm_do_claim_page(page);

    } else {

        /* 임시조치 ; Page->Frame이 있다면 Page Fault가 발생하지 말았어야 하지 않을까? */
        PANIC("Already in the Frame");
        // FREE ALL RESOURCES (PROCESS EXIT)
    }

    /* Locate the page that faulted in the supplemental page table.
       If the memory reference is valid, use the supplemental page table entry to locate the data that goes in the page,
       which might be in the file system, or in a swap slot, or it might simply be an all-zero page.

       If you implement sharing (i.e., Copy-on-Write), the page's data might even already be in a page frame, but not in the page table.
       If the supplemental page table indicates that the user process should not expect any data at the address it was trying to access,
       or if the page lies within kernel virtual memory, or if the access is an attempt to write to a read-only page,
       then the access is invalid. Any invalid access terminates the process and thereby frees all of its resources.

       Obtain a frame to store the page. If you implement sharing, the data you need may already be in a frame,
       in which case you must be able to locate that frame.

       Fetch the data into the frame, by reading it from the file system or swap, zeroing it, etc.
       If you implement sharing, the page you need may already be in a frame, in which case no action is necessary in this step.

       Point the page table entry for the faulting virtual address to the physical page. You can use the functions in threads/mmu.c. */
}

/* Free the page. DO NOT MODIFY THIS FUNCTION. */
/* 페이지를 Free 해주는 함수로, 건드리지 말 것. */
void vm_dealloc_page(struct page *page) {

    destroy(page);
    free(page);
}

/* Claim the page that allocate on VA. */
/* 가상 주소를 기반으로 소속 페이지를 검색해보고, 없다면 페이지를 생성해서 DRAM의 Frame에 삽입하는 함수 (VM_ANON ; 스택용). */
bool vm_claim_page(void *va) {

    /* 가상주소를 기반으로 SPT에서 페이지를 찾고, 이미 있으면 False 리턴 */
    struct supplemental_page_table *spt = &thread_current()->spt;
    va = pg_round_down(va);
    if (spt_find_page(spt, va) != NULL) {
        return false;
    }

    /* 페이지 생성 및 SPT 삽입 */
    if (!vm_alloc_page(VM_ANON, va, true)) {
        return false;
    }

    struct page *page = spt_find_page(spt, va);
    if (page == NULL) {
        return false;
    }

    /* 찾은 페이지를 Frame과 연결해서 메모리에 넣기 */
    return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
/* 페이지를 vm_get_frame()으로 확보한 Frame과 연결하는 함수. */
static bool vm_do_claim_page(struct page *page) {

    /* Frame을 확보 */
    struct frame *frame = vm_get_frame();
    if (!frame) {
        return false;
    }

    /* 확보한 Frame을 연결 */
    frame->page = page;
    page->frame = frame; // frame의 kva는 frame table에서 관리해야 할 것 같음

    /* SPT가 아닌 실제 PT에 PTE를 생성/삽입해서 연결 */
    if (!install_page(page->va, frame->kva, page->writable)) {
        palloc_free_page(frame->kva);
        return false;
    }

    /* 페이지 삽입 */
    bool result = swap_in(page, frame->kva);
    return result;

    // uint64_t *pml4 = thread_current()->pml4;
    // if (!pml4_set_page(pml4, page->va, frame->kva, true)) { // pml4_set_page(uint64_t *pml4, void *upage, void *kpage, bool rw)
    //     frame->page = NULL;
    //     page->frame = NULL;
    //     return false;
    // }
    // /* 페이지를 frame에 삽입하는 과정으로 보임 (조금 찝찝한데 원래 있던 코드) */
    // return swap_in(page, frame->kva);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////// Supplemental Page Table /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt) {

    /* SPT를 초기화하는 함수 (thread->spt && page->spt_hash_elem) ; page method */

    if (!spt) {
        PANIC("Supplemental page table pointer is NULL.");
        return;
    }

    hash_init(&spt->spt_hash_table, page_hash_func, page_less_func, NULL);
}

/* Copy supplemental page table from src to dst */
/* __do_fork()에서 사용되며, SPT를 복제하는 함수 ; 복사가 아닌 복제! */
bool supplemental_page_table_copy(struct supplemental_page_table *dst, struct supplemental_page_table *src) {

    /* hash.c를 보면, 해시테이블 내용물을 순회하는데에 hash_first를 정의 후 while hash_next를 활용해야 함 */
    struct hash_iterator i;
    hash_first(&i, &src->spt_hash_table);

    while (hash_next(&i)) {

        /* 먼저 부모의 Page 구조체를 확보하고, 새로운 페이지를 만들기 (단순히 페이지 복사가 아닌 복제니까) */
        struct page *parent_page = hash_entry(i.elem, struct page, spt_hash_elem);
        struct page *new_page = (struct page *)calloc(1, sizeof(struct page));
        if (new_page == NULL) {
            return false;
        }

        /* 부모의 페이지가 Frame을 가지고 있을 경우 (DRAM), 관련 데이터도 자식에 할당해서 복사 */
        if (parent_page->frame != NULL) {

            /* 새로운 페이지를 초기화 */
            uninit_new(new_page, parent_page->va, parent_page->uninit.init, parent_page->uninit.type, NULL, parent_page->uninit.page_initializer);
            new_page->writable = parent_page->writable;

            /* 새로운 페이지를 위한 프레임 확보 */
            struct frame *new_frame = vm_get_frame();
            if (new_frame == NULL) {
                free(new_page);
                return false;
            }

            /* 확보한 페이지와 프레임을 서로 연결 (DRAM에 할당) */
            new_page->frame = new_frame;
            new_frame->page = new_page;

            /* 새로운 페이지의 Initializer 정보 업데이트 */
            new_page->uninit.page_initializer(new_page, new_page->uninit.type, new_frame->kva);
            memcpy(new_frame->kva, parent_page->frame->kva, PGSIZE);
            install_page(new_page->va, new_frame->kva, new_page->writable); // 페이지테이블에 삽입
        }

        /* 부모의 페이지가 Frame이 없다면, 디스크 (Swap Table) 또는 Lazy Loading 상태 */
        else {

            /* @@@@@@@@@@ TODO : DISK에 있는지 찾아봐야 함 (SWAP 대응) @@@@@@@@@@ */

            /* 먼저 텅빈 lazy_load_aux 구조체를 선언하고 메모리 확보 */
            struct lazy_load_aux *info = (struct lazy_load_aux *)malloc(sizeof(struct lazy_load_aux));
            if (info == NULL) {
                free(new_page);
                return false;
            }

            /* 부모의 구조체를 확보해서 값을 복제 */
            struct lazy_load_aux *parent_info = (struct lazy_load_aux *)parent_page->uninit.aux;
            if (parent_info != NULL) {
                memcpy(info, parent_info, sizeof(struct lazy_load_aux));
            }

            /* 확보한 구조체를 기반으로 페이지 초기화 작업 수행 (당연한 말이지만 Frame에는 넣을 필요 없음) */
            uninit_new(new_page, parent_page->va, parent_page->uninit.init, parent_page->uninit.type, info, parent_page->uninit.page_initializer);
            new_page->writable = parent_page->writable;
        }

        /* Destination SPT에 새로 만든 페이지를 복사하는데, 실패시 Free 해주기 */
        if (!spt_insert_page(dst, new_page)) {

            /* 만일 프레임을 가지고 있었다면 프레임 Dealloc */
            if (new_page->frame) {
                /* @@@@@@@@@@ TODO @@@@@@@@@@ */
                // vm_dealloc_frame(new_page->frame) 형태로, 프레임 제거 (evict 완성후 채우기)
            }

            /* 프레임 유무와 관계없이 페이지와 관련된 메모리 Free */
            free(new_page);
            return false;
        }
    }

    // /* 복사 작업이 끝났는데 뭔가 안맞는다면 Copy Failed 에러 주기 */
    if (dst->spt_hash_table.elem_cnt != src->spt_hash_table.elem_cnt) {
        PANIC("Supplementary Page Table Copy Failed");
    }

    return true;
}

/* Free the resource hold by the supplemental page table */
/* SPT에 속한 페이지를 전부 삭제하고, 수정된 컨텐츠들을 디스크에 다시 저장하는 함수. */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED) {

    /* 해시테이블을 순회하면서 모든 페이지에 destroy_page()를 적용 */
    hash_destroy(&spt->spt_hash_table, destroy_page);
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Hashtable Functions ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

uint64_t page_hash_func(const struct hash_elem *e, void *aux UNUSED) {

    const struct page *p = hash_entry(e, struct page, spt_hash_elem);

    return hash_bytes(&p->va, sizeof p->va);
}

bool page_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {

    const struct page *aa = hash_entry(a, struct page, spt_hash_elem);
    const struct page *bb = hash_entry(b, struct page, spt_hash_elem);

    return aa->va < bb->va;
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// SPT Support Functions /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* 해당 페이지가 페이지테이블에 없을 경우 저장하는 함수 */
static bool install_page(void *upage, void *kpage, bool writable) {

    struct thread *t = thread_current();
    // lock_acquire(&page_table_lock);
    bool result = (pml4_get_page(t->pml4, upage) == NULL && pml4_set_page(t->pml4, upage, kpage, writable));
    // lock_release(&page_table_lock);

    /* Upage로 대변되는 가상주소에 페이지가 없는지 확인하고, 맞다면 페이지테이블에 삽입 */
    return result;
}

/* supplemental_page_table_kill()에서 사용됨 */
void destroy_page(struct hash_elem *e, void *aux) {

    struct page *page = hash_entry(e, struct page, spt_hash_elem);

    vm_dealloc_page(page);
}