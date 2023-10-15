#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer(struct page *, void *aux);

/* 아직 초기화가 안된 페이지. Lazy Loading이 적용되는 페이지. */
struct uninit_page {

    /* 페이지의 컨텐츠들을 초기화 */
    vm_initializer *init;
    enum vm_type type;
    void *aux;

    /* Struct Page를 초기화하고 PA-VA 매핑 작업 수행 */
    bool (*page_initializer)(struct page *, enum vm_type, void *kva);
};

void uninit_new(struct page *page, void *va, vm_initializer *init, enum vm_type type, void *aux, bool (*initializer)(struct page *, enum vm_type, void *kva));
bool uninit_init(struct page *page, void *kva);
#endif
