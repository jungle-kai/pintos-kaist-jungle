/* Uses a memory mapping to read a file. */

#include <string.h>
#include <syscall.h>
#include "tests/vm/sample.inc"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void)
{
  char *actual = (char *) 0x10000000;
  int handle;
  void *map;
  size_t i;

  CHECK ((handle = open ("sample.txt")) > 1, "open \"sample.txt\"");

  // 이때는 spt에 struct page만 넣어주는 단계
  CHECK ((map = mmap (actual, 4096, 0, handle, 0)) != MAP_FAILED, "mmap \"sample.txt\"");

  /* Check that data is correct. */

  // 이때는 *actual로 값에 접근하기 때문에 page fault 발생.
  // page_fault handler 호출된 후, 파일데이터 로딩.
  // memcmp: actual과 sample을 시작지점으로, sample길이 만큼 비교했을 때 다 같으면 0 리턴. 같지 않으면 1 리턴.
  // -> 같지 않을 시 fail
  if (memcmp (actual, sample, strlen (sample))) {
    fail ("read of mmap'd file reported bad data");
  }

  /* Verify that data is followed by zeros. */
  for (i = strlen (sample); i < 4096; i++)
    if (actual[i] != 0)
      fail ("byte %zu of mmap'd region has value %02hhx (should be 0)",
            i, actual[i]);

  munmap (map);
  close (handle);
}
