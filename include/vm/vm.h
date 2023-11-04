// clang-format off
#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include "threads/palloc.h"
#include "lib/kernel/bitmap.h"
// clang-format on

enum vm_type {
    /* page not initialized */
    /* 초기화되지 않은 페이지 */
    VM_UNINIT = 0,
    /* page not related to the file, aka anonymous page */
    /* 파일과 관련 없는 페이지(익명 페이지) */
    VM_ANON = 1,
    /* page that realated to the file */
    /* 파일과 관련된 페이지 */
    VM_FILE = 2,
    /* page that hold the page cache, for project 4 */
    /* 페이지 캐시 페이지. */
    VM_PAGE_CACHE = 3,

    /* Bit flags to store state */

    /* Auxillary bit flag marker for store information. You can add more
     * markers, until the value is fit in the int. */
    VM_MARKER_0 = (1 << 3),
    VM_MARKER_1 = (1 << 4),

    /* DO NOT EXCEED THIS VALUE. */
    /* 이 값을 초과하면 안됨 */
    VM_MARKER_END = (1 << 31),
};

#include "vm/anon.h"
#include "vm/file.h"
#include "vm/uninit.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/list.h"
#ifdef EFILESYS
#include "filesys/page_cache.h"
#endif



#define VM_TYPE(type) ((type)&7)

/* The representation of "page".
 * This is kind of "parent class", which has four "child class"es, which are
 * uninit_page, file_page, anon_page, and page cache (project4).
 * DO NOT REMOVE/MODIFY PREDEFINED MEMBER OF THIS STRUCTURE. */
 /* 가상페이지 정보를 담는 보조 구조체로 page 만들어 사용.
	이 page 구조체는 부모 클래스같은 것이다. 
	그리고 4개의 자식 클래스 중 하나가 상속받아 사용할 수 있다.
	각각 uninit_page, file_page, anon_page, page cache */
struct page {
    const struct page_operations *operations;
    void *va;            /* Address in terms of user space */ /* 유저 공간의 가상페이지 시작주소*/
    struct frame *frame; /* Back reference for frame */ /* 페이지와 연결된 프레임 */

    /* Per-type data are binded into the union.
     * Each function automatically detects the current union */
    /* 유니온 영역은 4개 페이지 구조체 중 하나가 된다 */

    /***** 추가한 필드 *****/
    struct hash_elem spt_hash_elem;
    struct list_elem frame_share_elem;
    bool writable;
    uint64_t* pml4;
    enum vm_type PAGE_TYPE;
    /***** 추가한 필드 *****/
    
    union {
        struct uninit_page uninit; // 페이지가 사용할 수 있는 operation들. 페이지 타입별로 다른 구현체
        struct anon_page anon;
        struct file_page file;
#ifdef EFILESYS
        struct page_cache page_cache;
#endif
    };
};

/* The representation of "frame" */
/* 프레임 구조체 */

struct frame {
    void *kva; // kernel virtual address. 커널 가상 주소. palloc_get_page()의 반환값 넣어줌
    struct page *page; // 프레임과 연결된 가상페이지의 보조 page struct
    struct list_elem frame_elem;
    struct list pages;
    size_t share_cnt;
};




/* The function table for page operations.
 * This is one way of implementing "interface" in C.
 * Put the table of "method" into the struct's member, and
 * call it whenever you needed. */

/* 페이지 operations에 대한 함수 테이블
	C에서 인터페이스를 구현하는 방법중 하나이다.
	구조체 멤버변수에 method들(함수들)를 넣고 필요할 때마다 call 
	swap in(메인 메모리에 넣기), swap out(메인 메모리에서 뺀 후, 스왑 저장소에 넣기), destroy(페이지 할당해제), 페이지의 타입*/
struct page_operations {
    bool (*swap_in)(struct page *, void *);
    bool (*swap_out)(struct page *);
    void (*destroy)(struct page *);
    enum vm_type type;
};

/* 해당 페이지 특화된 operation을 호출해주는 매크로 함수 */
#define swap_in(page, v) (page)->operations->swap_in((page), v)
#define swap_out(page) (page)->operations->swap_out(page)
#define destroy(page)                                                                                                                                                                                  \
    if ((page)->operations->destroy)                                                                                                                                                                   \
    (page)->operations->destroy(page)

/* Representation of current process's memory space.
 * We don't want to force you to obey any specific design for this struct.
 * All designs up to you for this. */
/* 현재 프로세스의 메모리 공간의 표현
	이 구조체를 위해 어떤 특정 디자인을 고수할 필요는 없다.
	모든 디자인은 너에게 달렸다. */
struct supplemental_page_table {
    // page들을 저장할 곳 필요 -> hash 사용
	// hash 초기화 필요, hash할 데이터 = page
	struct hash hash;
};



#include "threads/thread.h"
extern struct list frame_table;
void destory_page (struct hash_elem *e, void *aux);
void supplemental_page_table_init(struct supplemental_page_table *spt);
bool supplemental_page_table_copy(struct supplemental_page_table *dst, struct supplemental_page_table *src);
void supplemental_page_table_kill(struct supplemental_page_table *spt);
struct page *spt_find_page(struct supplemental_page_table *spt, void *va);
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page);
void spt_remove_page(struct supplemental_page_table *spt, struct page *page);

void vm_init(void);
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user, bool write, bool not_present);

#define vm_alloc_page(type, upage, writable) vm_alloc_page_with_initializer((type), (upage), (writable), NULL, NULL)
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable, vm_initializer *init, void *aux);
void vm_dealloc_page(struct page *page);
bool vm_claim_page(void *va);
enum vm_type page_get_type(struct page *page);
// void swap_table_init(struct swap_table *swapt);
// void swap_table_kill(struct swap_table *swapt);
#endif /* VM_VM_H */
