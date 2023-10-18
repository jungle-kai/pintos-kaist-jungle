/* Generates about 1 MB of random data that is then divided into
   16 chunks.  A separate subprocess sorts each chunk; the
   subprocesses run in parallel.  Then we merge the chunks and
   verify that the result is what it should be. */

#include "tests/vm/parallel-merge.h"
#include <stdio.h>
#include <syscall.h>
#include "tests/arc4.h"
#include "tests/lib.h"
#include "tests/main.h"

#define CHUNK_SIZE (128 * 1024)
#define CHUNK_CNT 8                             /* Number of chunks. */
#define DATA_SIZE (CHUNK_CNT * CHUNK_SIZE)      /* Buffer size. */

unsigned char buf1[DATA_SIZE], buf2[DATA_SIZE];
size_t histogram[256];

/* Initialize buf1 with random data,
   then count the number of instances of each value within it. */
static void
init (void)
{
  struct arc4 arc4;
  size_t i;

  msg ("init");

  arc4_init (&arc4, "foobar", 6); // 랜덤값 세팅
  arc4_crypt (&arc4, buf1, sizeof buf1); // buf1을 세팅한 랜덤값으로 다 채움
  for (i = 0; i < sizeof buf1; i++)
    histogram[buf1[i]]++; // 랜덤값에 어떤 문자가 분포되어 있는지 저장
}

/* Sort each chunk of buf1 using SUBPROCESS,
   which is expected to return EXIT_STATUS. */
static void
sort_chunks (const char *subprocess, int exit_status)
{
  pid_t children[CHUNK_CNT];
  size_t i;

  for (i = 0; i < CHUNK_CNT; i++)
    {
      char fn[128];
      char cmd[128];
      int handle;

      msg ("sort chunk %zu", i); // (page-merge-stk) sort chunk 0,1,2,3,...

      /* Write this chunk to a file. */
      snprintf (fn, sizeof fn, "buf%zu", i); // 파일 이름 생성: buf0,buf1,...
      create (fn, CHUNK_SIZE); // 디스크에 파일 공간 확보 및 생성
      quiet = true;
      CHECK ((handle = open (fn)) > 1, "parent_open1 \"%s\"", fn); // fn(buf0, buf1, buf2, ...)을 이름으로 가지는 파일 디스크에서 찾아서 열기(struct file* file 만들기)
      write (handle, buf1 + CHUNK_SIZE * i, CHUNK_SIZE); // buf1을 chunk_size단위로 잘라 handle 파일에 write
      close (handle); // 파일 닫기(struct file free, fd 초기화, file 참조:0 이면 inode도 해제)

      // fn 파일에 chunk write되었음.

      /* Sort with subprocess. */
      snprintf (cmd, sizeof cmd, "%s %s", subprocess, fn); // subprocess: child-qsort, cmd = "child-qsort buf0", "child-qsort buf1", ... "child-qsort buf7"  
      children[i] = fork (subprocess); // fork("child-qsort")의 반환값 // 0이면 자식, 아니면 부모

      // 자식일 때, exec("child-qsort bufX") 하고 끝
      if (children[i] == 0) {
        // printf("나는자식\n");
        CHECK ((children[i] = exec (cmd)) != -1, "exec \"%s\"", cmd);
      }

      // printf("자식의pid: %d\n", children[i]);

      // 부모일 때는 바로 여기로 와서 for문 돌러감.
      quiet = false;
    }

  // 부모는 위 for문 다 돌고 여기로 옴
  for (i = 0; i < CHUNK_CNT; i++)
    {
      char fn[128];
      int handle;

      // children[i]에는 자식의 pid 저장해놨음. 자식이 종료될 때까지 기다렸다가 종료되면서 exit_status 반환하면, 반환값과 exit_status 비교해서 잘 종료되었으면 wait for child 0,1,2...7 호출
      CHECK (wait (children[i]) == exit_status, "wait for child %zu", i);

      /* Read chunk back from file. */
      quiet = true;
      snprintf (fn, sizeof fn, "buf%zu", i); // fn = buf0, buf1, ... , 첫번째 for문에서 만든 파일
      CHECK ((handle = open (fn)) > 1, "parent_open2 \"%s\"", fn); // 파일 열리는지 체크
      read (handle, buf1 + CHUNK_SIZE * i, CHUNK_SIZE); // 열렸으면 buf1에다가 쓰기
      close (handle); // 파일 닫기
      quiet = false;
    }
}

/* Merge the sorted chunks in buf1 into a fully sorted buf2. */
static void
merge (void)
{
  unsigned char *mp[CHUNK_CNT];
  size_t mp_left;
  unsigned char *op;
  size_t i;

  msg ("merge");

  /* Initialize merge pointers. */
  mp_left = CHUNK_CNT;
  for (i = 0; i < CHUNK_CNT; i++)
    mp[i] = buf1 + CHUNK_SIZE * i;

  /* Merge. */
  op = buf2;
  while (mp_left > 0) // 병합될 청크가 없을 때까지 수행
    {
      /* Find smallest value. */
      // 청크 각각의 문자열은 이미 자식들이 알파벳순으로 정렬해놓았고, 이제 시작문자열이 가장 낮은 알파벳을 가지는 청크를 뽑아내서 op에 해당 청크의 시작문자를 넣는다.
      size_t min = 0;
      for (i = 1; i < mp_left; i++)
        if (*mp[i] < *mp[min])
          min = i;

      /* Append value to buf2. */
      *op++ = *mp[min]; // 청크의 시작 문자를 op에 저장, op 포인터 한칸 이동.

      /* Advance merge pointer.
         Delete this chunk from the set if it's emptied. */
      // mp[min]에 해당하는 청크를 가리키는 포인터 한칸 이동
      // 한칸씩 이동하다가, 이동한 거리가 chunk_size만큼 되었다면,
      // 해당 청크는 다 끝난것임.
      // 따라서, 
      if ((++mp[min] - buf1) % CHUNK_SIZE == 0) // 
        mp[min] = mp[--mp_left];
    }
}

static void
verify (void)
{
  size_t buf_idx;
  size_t hist_idx;

  msg ("verify");

  buf_idx = 0;
  for (hist_idx = 0; hist_idx < sizeof histogram / sizeof *histogram;
       hist_idx++)
    {
      while (histogram[hist_idx]-- > 0)
        {
          if (buf2[buf_idx] != hist_idx)
            fail ("bad value %d in byte %zu", buf2[buf_idx], buf_idx);
          buf_idx++;
        }
    }

  msg ("success, buf_idx=%'zu", buf_idx);
}

void
parallel_merge (const char *child_name, int exit_status)
{
  init ();
  sort_chunks (child_name, exit_status);
  merge ();
  verify ();
}
