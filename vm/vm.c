/* vm.c: Generic interface for virtual memory objects. */

// clang-format off
#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include <hash.h> // SPT 해시테이블을 위해서 추가

// #define VM
// clang-format on

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

    /* @@@@@@@@@@ TODO: Your code goes here. @@@@@@@@@@ */
}

/* Get the type of the page. This function is useful if you want to know the
   type of the page after it will be initialized. This function is fully implemented now. */
enum vm_type page_get_type(struct page *page) {

    /* 이미 부여된 페이지의 종류를 반환하는 함수.
       초기화 이후의 Type를 반환하며, 수정 대상 아님. */

    int ty = VM_TYPE(page->operations->type);

    switch (ty) {
    case VM_UNINIT:
        return VM_TYPE(page->uninit.type);
    default:
        return ty;
    }
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a page,
   do not create it directly and make it through this function or `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {

    /* 핀토스에서 모든 페이지는 처음 생성될 때 이 함수로 초기화/생성 되어야 함.
       대안 함수인 vm_alloc_page()는 마크로 <vm_alloc_page(type, upage, writable)>로 이 함수를 부름.
       upage는 USERPAGE를 의미함 ; 커널이 아닌 유저 공간의 가상 주소로, 페이지의 시작 주소 */

    ASSERT(VM_TYPE(type) != VM_UNINIT)

    struct supplemental_page_table *spt = &thread_current()->spt;

    /* Check wheter the upage is already occupied or not. */
    if (spt_find_page(spt, upage) == NULL) {

        /* TODO: Create the page, fetch the initialier according to the VM type,
                 and then create "uninit" page struct by calling uninit_new. You
                 should modify the field after calling the uninit_new. */

        /* @@@@@@@@@@ TODO: Insert the page into the spt. @@@@@@@@@@ */
    }
err:
    return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED) {

    /* Parameter로 제공된 Supplementary Page Table (SPT)에서 페이지의 가상주소를 찾고 반환하는 함수. */

    struct page *page = NULL;

    /* @@@@@@@@@@ TODO: Fill this function. @@@@@@@@@@ */

    return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {

    /* 페이지의 멤버인 Supplementary Page Table (SPT)에 페이지를 등록하는 함수. */

    int succ = false;

    /* @@@@@@@@@@ TODO: Fill this function. @@@@@@@@@@ */

    return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page) {

    /* 페이지의 SPT에서 특정 페이지를 제거하는 함수 */

    vm_dealloc_page(page);
    return true;
}

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

    struct frame *frame = NULL;

    /* @@@@@@@@@@ TODO: Fill this function. @@@@@@@@@@ */

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

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {

    /* Page Fault 발생 시 처음 Invoke 되는 함수.
       Fault 발생 사유에 따라서 어떤 조치가 필요한지 확인하고, 해당 액션을 통해서 페이지를 확보해오는 함수. */

    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
    struct page *page = NULL;

    /* @@@@@@@@@@ TODO: Validate the fault @@@@@@@@@@ */
    /* @@@@@@@@@@ TODO: Your code goes here @@@@@@@@@@ */

    /* (1) Locate the page that faulted in the supplemental page table.
       If the memory reference is valid, use the supplemental page table entry to locate the data that goes in the page,
       which might be in the file system, or in a swap slot, or it might simply be an all-zero page.
       If you implement sharing (i.e., Copy-on-Write), the page's data might even already be in a page frame, but not in the page table.
       If the supplemental page table indicates that the user process should not expect any data at the address it was trying to access,
       or if the page lies within kernel virtual memory, or if the access is an attempt to write to a read-only page,
       then the access is invalid. Any invalid access terminates the process and thereby frees all of its resources.

       (2) Obtain a frame to store the page. If you implement sharing, the data you need may already be in a frame,
       in which case you must be able to locate that frame.

       (3) Fetch the data into the frame, by reading it from the file system or swap, zeroing it, etc.
       If you implement sharing, the page you need may already be in a frame, in which case no action is necessary in this step.

       (4) Point the page table entry for the faulting virtual address to the physical page. You can use the functions in threads/mmu.c. */

    return vm_do_claim_page(page);
}

/* Free the page.
   DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page) {

    /* 페이지를 Free 해주는 함수로, 건드리지 말 것. */

    destroy(page);
    free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED) {

    /* 가상 주소를 기반으로 소속 페이지를 찾고, DRAM의 Frame과 연결하여 사실상 메모리에 넣는 함수.
       이 함수가 항상 먼저 불릴 것 같고, 여기서 페이지를 찾은 뒤 do_claim_page()로 넘어감. */

    struct page *page = NULL;

    /* @@@@@@@@@@ TODO: Fill this function @@@@@@@@@@ */

    return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page(struct page *page) {

    /* vm_claim_page()에서 찾은 페이지를 실제 vm_get_frame()으로 확보한 Frame과 연결하는 함수. */

    struct frame *frame = vm_get_frame();

    /* Set links */
    frame->page = page;
    page->frame = frame;

    /* @@@@@@@@@@ TODO: Insert page table entry to map page's VA to frame's PA. @@@@@@@@@@ */

    return swap_in(page, frame->kva);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////// Supplemental Page Table /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {

    /*  */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */

    /* You may organize the supplemental page table as you wish.
       There are at least two basic approaches to its organization: in terms of segments or in terms of pages.

       A segment here refers to a consecutive group of pages, i.e., memory region containing an executable or a memory-mapped file.

       Optionally, you may use the page table itself to track the members of the supplemental page table.
       You will have to modify the Pintos page table implementation in threads/mmu.c to do so.
       We recommend this approach for advanced students only. */
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED, struct supplemental_page_table *src UNUSED) {

    /*  */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED) {

    /* SPT에 속한 페이지를 전부 삭제하고, 수정된 컨텐츠들을 디스크에 다시 저장하는 함수. */

    /* @@@@@@@@@@ TODO: Destroy all the SPT held by the current thread and writeback all the modified contents to the storage @@@@@@@@@@ */
}

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// Hashtable Functions ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

uint64_t hash_hash_func(const struct hash_elem *e, void *aux UNUSED) {
    const struct page *p = hash_entry(e, struct page, spt_hash_elem);
    return hash_bytes(&p->va, sizeof p->va);
}

/* Compares the value of two hash elements A and B, given
 * auxiliary data AUX.  Returns true if A is less than B, or
 * false if A is greater than or equal to B. */
bool hash_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    const struct page *aa = hash_entry(a, struct page, spt_hash_elem);
    const struct page *bb = hash_entry(b, struct page, spt_hash_elem);

    return aa->va < bb->va;
}
