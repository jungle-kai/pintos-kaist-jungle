#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);

/* 스택 성장을 위한 범위 정의 */
#define STACK_RESERVED_SIZE (1 * 1024 * 1024) // 1MB

/* Static 떼고 여기 선언 */
bool load_segment(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

#endif /* userprog/process.h */
