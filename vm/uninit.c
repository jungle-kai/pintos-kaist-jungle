/* uninit.c: Implementation of uninitialized page.
   All of the pages are born as uninit page. When the first page fault occurs, the handler chain calls uninit_initialize (page->operations.swap_in).
   The uninit_initialize function transmutes the page into the specific page object (anon, file, page_cache) by initializing the page object,
   and calls ㅑnitialization callback that was passed from vm_alloc_page_with_initializer function. */

/* 핀토스의 모든 페이지들은 UNINIT 페이지로 처음 시작하며, 유저 프로그램이나 커널이 접근하기 전까지는 초기화가 다 안된 Uninit Page 상태로 대기함.
   따라서 최초의 PageFault가 발생하면 핸들러 체인이 uninit_initialize()를 호출함.
   이 함수는 Parameter로 주어진 페이지를 Anon, File, Page_Cache 타입에 맞는 페이지 Object로 초기화한 뒤,
   vm_alloc_page_with_initializer()에서 전달된 callback을 호출하며 종료됨. */

// clang-format off
#include "vm/vm.h"
#include "vm/uninit.h"
#include <hash.h> // SPT 해시테이블을 위해서 추가

// #define VM
// clang-format on

static bool uninit_initialize(struct page *page, void *kva);
static void uninit_destroy(struct page *page);

/* DO NOT MODIFY this struct */
/* 이 구조체 변형 금지 */
static const struct page_operations uninit_ops = {
    .swap_in = uninit_initialize,
    .swap_out = NULL,
    .destroy = uninit_destroy,
    .type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
/* 이 함수 변형 금지 
	인자: [페이지, 가상주소, 초기자, 페이지 타입, 초기자의 인자, 초기화 함수]*/
void uninit_new(struct page *page, void *va, vm_initializer *init, enum vm_type type, void *aux, bool (*initializer)(struct page *, enum vm_type, void *)) {

    /* 새로운 Uninit 페이지를 초기화하는 함수로, 프로그램/커널에 의한 첫 접근이 있어야 작업이 마무리 됨.
       따라서 page_initializer 멤버로 일종의 임시저장 상태 진입. */

    ASSERT(page != NULL);

   // 페이지 생성
    *page = (struct page){.operations = &uninit_ops, /* operation: uninit specific한 ops 할당 */
                          .va = va, // 가상페이지의 시작주소
                          .frame = NULL, /* no frame for now */ /* 페이지 처음 생성시엔 연결된 프레임 없음 */
                          
                          /* page 구조체의 union 타입 부분.
		                  	union 영역을 uninit_page 구조체가 차지함 */
                          .uninit = (struct uninit_page){
                              .init = init,
                              .type = type,
                              .aux = aux,
                              .page_initializer = initializer,
                          }};
}

/* Initalize the page on first fault */
/* 첫 page fault가 발생하면 uninit_initialize() 호출됨 */
static bool uninit_initialize(struct page *page, void *kva) {

    /* Uninit_new()로 생성된 페이지에 최초로 PageFault 발생 시 핸들러가 호출하는 함수.
       Paramter로 전달받은 페이지를 Anon, File, Cache 전용 오브젝트로 변환하고 초기화 시킴.
       uninit_new()에서 저장했던 initializer 멤버를 활용함. */

    struct uninit_page *uninit = &page->uninit;

    /* Fetch first, page_initialize may overwrite the values */
    /* uninit_new()로 페이지 생성할 때 넣었던 vm_initializer를 받아옴 */
    vm_initializer *init = uninit->init;
    void *aux = uninit->aux;

    /* @@@@@@@@@@ TODO: You may need to fix this function. @@@@@@@@@@ */

      // 일단 페이지 타입 별 초기화(page_initializer) 해주고, lazy_load_segment에 해당하는 init() 함수 호출
   bool res = uninit->page_initializer(page, uninit->type, kva) && (init ? init(page, aux) : true); 
   return res;
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void uninit_destroy(struct page *page) {

    /* 프로세스의 구동 과정에서 다양한 VM_UNINIT 페이지들이 생성될 수 있는데, 대부분의 경우 바로 활용이 됨.
       다만 활용되지 않은 페이지들이 있을 수 있어, 이 함수를 통해서 리소스를 풀어줘야 함. */

    struct uninit_page *uninit UNUSED = &page->uninit;

    /* @@@@@@@@@@ TODO: Fill this function. If you don't have anything to do, just return. @@@@@@@@@@ */
}
