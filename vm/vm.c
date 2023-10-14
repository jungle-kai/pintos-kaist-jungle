/* vm.c: Generic interface for virtual memory objects. */

// clang-format off
#include "threads/malloc.h"
#include "threads/vaddr.h"
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
    // list_init(&frame_list);

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
static bool install_page(void *upage, void *kpage, bool writable);
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED);
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED);
/* Create the pending page object with initializer. If you want to create a page,
   do not create it directly and make it through this function or `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux) {

    /* 핀토스에서 모든 페이지는 처음 생성될 때 이 함수로 초기화/생성 되어야 함.
       대안 함수인 vm_alloc_page()는 마크로 <vm_alloc_page(type, upage, writable)>로 이 함수를 부름.
       upage는 USERPAGE를 의미함 ; 커널이 아닌 유저 공간의 가상 주소로, 페이지의 시작 주소 */

    ASSERT(VM_TYPE(type) != VM_UNINIT)

    struct supplemental_page_table *spt = &thread_current()->spt;

    /* Check wheter the upage is already occupied or not. */
    /* upage 가상주소가 이미 페이지로 사용중인지 확인해서 미사용중일 때만 정상 수행 */
    bool result = false;

    // 유저 가상주소가 키값이다. 한 페이지 내의 0~4095 사이의 주소로 hash를 찔렀을 때 항상 같은 값이 나와야 한다.
    // 따라서, 12비트만큼을 버림한 주소값을 hash의 키값으로 해야 한다.
    upage = pg_round_down(upage);
    if (spt_find_page(spt, upage) == NULL) {

        /* TODO: Create the page, fetch the initialier according to the VM type,
                 and then create "uninit" page struct by calling uninit_new. You
                 should modify the field after calling the uninit_new. */

        /* 순서 
			1. 페이지를 생성해라.
			2. 페이지 타입에 맞는 초기화 함수를 fetch해라.
			3. uninit_new() 함수를 호출해서 uninit page 구조체를 생성해라.
			4. uninit_new 함수를 호출한 후에야 필드를 변형할수 있다.
			5. 해당 페이지를 spt에 넣는다.
		*/

        /* @@@@@@@@@@ TODO: Insert the page into the spt. @@@@@@@@@@ */

        // 페이지 생성
		struct page* page = (struct page*)calloc(1, sizeof(struct page));
		// struct page* page = (struct page*)palloc_get_page(PAL_USER);

        // 페이지 구조체 생성 실패시 처리
        if (page == NULL) {
            ;
        }
		// 타입별로 초기화 함수 호출해서 page 초기화


        switch (type) {
            case VM_ANON:
        		uninit_new(page, upage, init, type, aux, anon_initializer);
                break;
            case VM_FILE:
        		uninit_new(page, upage, init, type, aux, file_backed_initializer);
                break;
            default:
                uninit_new(page, upage, init, type, aux, NULL);
                break;
        }

        // writable 추가
        page->writable = writable;

		// spt에 있는 해시에 page 추가
        result = spt_insert_page(spt, page);
        return result;
    }
err:
    return false;
}

/* Find VA from spt and return page. On error, return NULL. */
/* spt와 va를 이용해서 페이지를 찾아 리턴해라. 
	찾지 못했을 경우, NULL을 리턴해라.
	인자: [spt의 주소, va]*/
struct page *spt_find_page(struct supplemental_page_table *spt UNUSED, void *va UNUSED) {

    /* Parameter로 제공된 Supplementary Page Table (SPT)에서 페이지의 가상주소를 찾고 반환하는 함수. */
    /* @@@@@@@@@@ TODO: Fill this function. @@@@@@@@@@ */
    struct page p;
	struct hash_elem *e;

	p.va = va;
	e = hash_find (&spt->hash, &p.spt_hash_elem);
	return e != NULL ? hash_entry (e, struct page, spt_hash_elem) : NULL;
}

/* Insert PAGE into spt with validation. */
/* spt에 page를 삽입하는데, 삽입 전에 validation 검사를 수행한 후 valid하다면 넣어라.
	인자: [spt의 주소, 페이지의 주소] */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED, struct page *page UNUSED) {

    /* 페이지의 멤버인 Supplementary Page Table (SPT)에 페이지를 등록하는 함수. */

    int succ = false;

    // 이미 들어간 적 있는 page인지 확인
    // 아래 코드처럼 쓰면 오류터짐
    // if(!spt_find_page(spt, page->va)) {
    //     return false;
    // }

    /* @@@@@@@@@@ TODO: Fill this function. @@@@@@@@@@ */
    struct hash_elem* hash_e = hash_insert(&spt->hash, &page->spt_hash_elem);
	/* 이 함수를 채워라. */

    // hash_insert 성공시 반환값 == NULL
    if (hash_e == NULL) {
        succ = true;
    }

    return succ;
}

/* spt와 page를 받아서 page를 메모리 해제하는 함수
	인자: [spt의 주소, page의 주소] */
void spt_remove_page(struct supplemental_page_table *spt, struct page *page) {

    /* 페이지의 SPT에서 특정 페이지를 제거하는 함수 */

    vm_dealloc_page(page);
    return true;
}

/* Get the struct frame, that will be evicted. */
/* frame 구조체를 얻는다. 이 frame은 evicted될 것이다. */
static struct frame *vm_get_victim(void) {

    /* LRU 등 팀에서 정한 알고리즘을 활용, DRAM에서 쫒아낼 Present Upage를 선정하는 함수. */

    struct frame *victim = NULL;

    /* @@@@@@@@@@ TODO: The policy for eviction is up to you. @@@@@@@@@@*/

    return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
/* 한 페이지를 evict하고 그에 대응하는 frame을 리턴한다.
	에러 발생시 NULL 리턴한다. */
static struct frame *vm_evict_frame(void) {

    /* vm_get_victim()으로 선정한 페이지를 DRAM에서 쫒아내는 함수. */

    struct frame *victim UNUSED = vm_get_victim(); // evict될 victim frame 하나를 얻어온다.

    /* @@@@@@@@@@ TODO: swap out the victim and return the evicted frame. @@@@@@@@@@ */
    /* victim frame을 swap out하고 리턴한다. */
    return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/

/* palloc_get_page() 해서 물리메모리 페이지 할당받아 frame을 얻는다. 사용할 수 있는 페이지가 없을 때는, page를 evict하고 그걸 리턴한다. 
	이 함수는 항상 유효한 주소값을 반환해야 한다. 유저풀 메모리가 꽉차면, 이 함수는 가용 메모리 공간을 얻기 위해 프레임을 evict한다.*/

static struct frame *vm_get_frame(void) {

    /* Frame Table을 탐색하고, 빈 Frame을 반환하는 함수.
       빈 Frame이 없다면 vm_evict_frame()으로 공간을 확보한 뒤에 반환. */

    /* @@@@@@@@@@ TODO: Fill this function. @@@@@@@@@@ */
    struct frame* frame = (struct frame*)calloc(1, sizeof(struct frame));
    if (frame == NULL) {
        return NULL;
    }

    frame->kva = palloc_get_page(PAL_USER | PAL_ZERO);

    // 페이지 할당 실패 -> evict policy 결과로 얻은 frame 반환
    if (frame->kva == NULL) {
        free(frame);
        frame = vm_evict_frame(); // vm_evict_frame() 반환되는 frame은 kva에 이미 물리페이지 주소가 들어있는가?
    }

    // list_push_back(&frame_list, &frame->frame_elem);

    ASSERT(frame != NULL);
    ASSERT(frame->page == NULL);
    return frame;
}

/* Growing the stack. */
/* 스택을 키운다. 
	인자: 가상주소*/
static void vm_stack_growth(void *addr UNUSED) {

    /* 스택의 크기를 키울 떄 사용하는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
}

/* Handle the fault on write_protected page */
/* 쓰기 보호가 되어 있는 페이지에 쓰려고 했을 때 호출되는 핸들러 
	인자: [쓰기 시도한 페이지]*/
static bool vm_handle_wp(struct page *page UNUSED) {

    /* Write_protect가 걸려있는 페이지에 접근하여 fault가 발생할 경우 Invoke 되는 함수. */

    /* @@@@@@@@@@ TODO @@@@@@@@@@ */
}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED, bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {

    /* Page Fault 발생 시 처음 Invoke 되는 함수.
       Fault 발생 사유에 따라서 어떤 조치가 필요한지 확인하고, 해당 액션을 통해서 페이지를 확보해오는 함수. */
    /* @@@@@@@@@@ TODO: Validate the fault @@@@@@@@@@ */

    struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
    struct page *page = NULL;

    /* 1. 유효한 페이지 폴트인지 검사 */
    // stack 생각, stack overflow 생각, stack이랑 멀리 떨어졌을 경우, code segment 접근 경우
    // ->  

    /* 2. 유효하다면, fault발생된 addr 가상 주소를 가지고 spt에 접근해서 page 구조체를 구한다. */
    // printf("페이지폴트 주소!!: %p\n", addr); 
    addr = pg_round_down(addr);
    // printf("ROUNDED 페이지폴트 주소!!: %p\n", addr); 
    
    page = spt_find_page(spt, addr);

    // page NULL 체크
    /* 3. page를 가지고 uninit_initialize() 함수 호출.
        호출시, 페이지 타입별 초기화 함수() -> lazy_load_segment() 호출됨 */ 

    // 유효한 page fault일 때,
    // page를 claim해서 frame을 만들어 page와 연결

    if (!page) {
        return false;
    }

    return vm_do_claim_page(page);

    // res = vm_do_claim_page(page);
    // return res;
    

    // 이제 uninit_init(page, kva) 해줌.
    // bogus fault인 경우, 이미 frame이 매핑된 상태이므로 바로 uninit_init() 해줌.
    // if (res) {
    //     uninit_init(page, page->frame->kva);
    //     return true;
    // }
    // return false;

    /* 4. bogus 폴트인지 확인
    (bogus 폴트: 지연 로딩으로 인해 물리 메모리와 매핑되어 있지만 콘텐츠 자체는 물리메모리에 로드되어 있지 않은 경우가 있을 수 있다. 따라서 물리 메모리와 매핑은 되어 있지만, 콘텐츠가 로드되어 있지 않은 경우라면, 콘텐츠를 로드하면 되고, 매핑되지 않은 페이지(PTE_P = 0)라면 유효한 페이지 폴트)
        - lazy_loading_page인 경우
        - swap_out_page인 경우
        - write_protected_page인 경우*/

    // 아직 page와 frame은 매핑되어 있지 않다. vm_initializer()만 호출한 상태이기 때문
    // 따라서, bogus fault가 아닌 상태(페이지와 프레임이 연결된 상태)와 유효한 페이지 폴트인 상태(연결된 프레임이 없음)을 구분하여 uninit_init() 호출 후, vm_do_claim_page() 호출




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

    
    // return vm_do_claim_page(page);
}

/* Free the page.
   DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page) {

    /* 페이지를 Free 해주는 함수로, 건드리지 말 것. */

    destroy(page);
    free(page);
}

/* Claim the page that allocate on VA. */
/* 진짜로 va에 대해 가상페이지 + 프레임 만들고 매핑시키는거까지 하라고 claim(주장)하는 것 */
bool vm_claim_page(void *va UNUSED) {

    /* 가상 주소를 기반으로 소속 페이지를 찾고, DRAM의 Frame과 연결하여 사실상 메모리에 넣는 함수.
       이 함수가 항상 먼저 불릴 것 같고, 여기서 페이지를 찾은 뒤 do_claim_page()로 넘어감. */

    /* @@@@@@@@@@ TODO: Fill this function @@@@@@@@@@ */

    // 우선, va에 대해 이미 만들어져 있던 page면 -1 리턴
    struct page *page = NULL;
    struct supplemental_page_table spt = thread_current()->spt;

    // spt에 들어가 있지 않았던 것일 때,
    va = pg_round_down(va);
    if (spt_find_page(&spt, va) == NULL) {
        // 페이지 구조체 만들고,
        page = (struct page*)calloc(1, sizeof(struct page));
        if (page == NULL) {
            return 0;
        }
        // 페이지가 위치한 va 연결시키고,
        page->va = va;
        // spt에 넣고,
        spt_insert_page(&spt, page);

        // 해당 페이지 구조체와 frame을 연결시킴
        return vm_do_claim_page(page);
    }
    // 들어가 있던거면 오류, 0 리턴
    return 0;
}

static bool install_page(void *upage, void *kpage, bool writable) {
    struct thread *t = thread_current();

    /* Verify that there's not already a page at that virtual
     * address, then map our page there. */
    return (pml4_get_page(t->pml4, upage) == NULL && pml4_set_page(t->pml4, upage, kpage, writable));
}

/* Claim the PAGE and set up the mmu. */
static bool vm_do_claim_page(struct page *page) {

    /* vm_claim_page()에서 찾은 페이지를 실제 vm_get_frame()으로 확보한 Frame과 연결하는 함수. */

    struct frame *frame = vm_get_frame();
    if (frame == NULL) {
        return 0;
    }

    /* Set links */
    frame->page = page;
    page->frame = frame;

    /* @@@@@@@@@@ TODO: Insert page table entry to map page's VA to frame's PA. @@@@@@@@@@ */
    // pml4_set_page(thread_current()->pml4, page->va, frame->kva, 1);
    // if (!install_page(page->va, frame->kva, 1)) {
    //     printf("fail\n");
    //     palloc_free_page(frame->kva);
    //     return false;
    // }
    
    bool res = swap_in(page, frame->kva);
    return res;
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED) {
    hash_init(&spt->hash, page_hash, page_less, NULL);
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
    dst->hash.elem_cnt = src->hash.elem_cnt;

    struct hash_iterator i;
    bool result = true;
    hash_first (&i, &src->hash);

    while (hash_next (&i)) {
        // 부모 page 구조체 얻음
        struct page *parent_page = hash_entry (i.elem, struct page, spt_hash_elem);

        // 새 page 만듬
        struct page* new_page = (struct page*)calloc(1, sizeof(struct page));

        // 새 page 초기화
        uninit_new(new_page, parent_page->va, parent_page->uninit.init, parent_page->uninit.type, parent_page->uninit.aux, parent_page->uninit.page_initializer);

        new_page->writable = parent_page->writable;
    
        // frame이 NULL이 아닐 때, 부모쪽 프레임과 연결된 물리메모리 데이터를 자식에도 할당해서 복사해줌
        if (parent_page->frame != NULL) {
            // 일단 frame 얻기
            struct frame* new_frame = vm_get_frame();

            if (new_frame == NULL) {
                return false;
            }

            // frame과 page 연결
            new_page->frame = new_frame;
            new_frame->page = new_page;

            // 페이지 초기화
            new_page->uninit.page_initializer(new_page, new_page->uninit.type, new_frame->kva);

            // 부모의 frame으로부터 kva 구한 후, kva에서 PGSIZE만큼 복사
            memcpy(new_frame->kva, parent_page->frame->kva, PGSIZE);

            // 페이지 테이블에 등록
            // install_page에 writable을 넣어주어야 함
            // page 구조체에 쓰기여부 넣기?
            // pml4e_walk로 구할 수 있나?
            install_page(new_page->va, new_frame->kva, new_page->writable);
            // // vm_do_claim_page(): page를 넣으면, frame을 만들고 연결시키고 lazy_loading까지 해주는 함수.
            // if (!vm_do_claim_page(new_page)) {
            //     printf("spt cpy: 페이지-프레임 연결실패!\n");
            //     return false;
            // }  

            // vm_do_claim_page()가 성공했으면 이후에 해줄거 없음.
        }

        // 다 했으면 이제 dst에 새 page넣기
        if (!spt_insert_page(dst, new_page)) {
            printf("spt cpy: spt 새페이지 insert 실패!\n");
            return false;
        }
    }

    // printf("sptcpy: 성공!\n");
    return true;

}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED) {

    /* SPT에 속한 페이지를 전부 삭제하고, 수정된 컨텐츠들을 디스크에 다시 저장하는 함수. */

    /* @@@@@@@@@@ TODO: Destroy all the SPT held by the current thread and writeback all the modified contents to the storage @@@@@@@@@@ */
}

/* Returns true if page a precedes page b. */
bool
page_less (const struct hash_elem *a_,
           const struct hash_elem *b_, void *aux UNUSED) {
  const struct page *a = hash_entry (a_, struct page, spt_hash_elem);
  const struct page *b = hash_entry (b_, struct page, spt_hash_elem);

  return a->va < b->va;
}

/* Returns a hash value for page p. */
unsigned
page_hash (const struct hash_elem *p_, void *aux UNUSED) {
  const struct page *p = hash_entry (p_, struct page, spt_hash_elem);
  return hash_bytes (&p->va, sizeof p->va);
}
