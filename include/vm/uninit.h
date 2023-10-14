#ifndef VM_UNINIT_H
#define VM_UNINIT_H
#include "vm/vm.h"

struct page;
enum vm_type;

typedef bool vm_initializer (struct page *, void *aux);

/* Uninitlialized page. The type for implementing the
 * "Lazy loading". */
/* 초기화되지 않은 페이지 구조체
	Lazy loading을 구현할 때 필요한 타입이다. 
	구조체 안에 vm_initializer과 page_initializer가 있는데 둘이 각각 어떤 역할을 하는지?
	일단 page_initializer: page를 초기화하고 pa와 kva를 연결시킴 -> 불확실*/

struct uninit_page {
	/* Initiate the contets of the page */
	vm_initializer *init;
	enum vm_type type;
	void *aux;
	/* Initiate the struct page and maps the pa to the va */
	bool (*page_initializer) (struct page *, enum vm_type, void *kva);
};

/* uninit_page 생성 함수 */      
void uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *kva));
#endif
bool uninit_init(struct page* page, void* kva);
