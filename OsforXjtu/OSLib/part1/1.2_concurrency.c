
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>

/*实验内容 （1）在进程中给一变量赋初值并成功创建两个线程；
（2）在两个线程中分别对此变量循环五千次以上做不同的操作（自行设计）
并输出结果；
（3）多运行几遍程序观察运行结果，如果发现每次运行结果不同，请解释原因并修改程序解决，考虑如何控制互斥和同步；
（4）将任务一中第一个实验调用 system 函数和调用 exec 族函数改成在线
程中实现，观察运行结果输出进程 PID 与线程 TID 进行比较并说明原因
*/

int count = 0;
pthread_mutex_t m;

void *
increment (void *arg)
{
  int result;
  char *myargs[] = { "wc", "Makefile", NULL };
  //获取pid
  printf ("get_pid in %s : %d\n", (char *)arg, getpid ());
  //获取tid
  printf ("get tid of %s : %ld\n", (char *)arg, syscall (SYS_gettid));
  pthread_mutex_lock (&m);
  for (int i = 0; i < 100000; i++)
    {
      count++;
    }
  result = count;
  pthread_mutex_unlock (&m);
  printf ("%s count = %d\n", (char *)arg, result);
  printf ("first call\n");
  //调用 编译好的system_call程序
  system ("./system_call");
  // 如何修改
  execvp (myargs[0], myargs);
  printf ("second call\n");
  system ("./system_call");
  return NULL;
}

int
main ()
{
  pthread_t p0, p1;
  int rc;
  char *name0 = "p0";
  char *name1 = "p1";
  printf ("current pid = %d\n", getpid ());
  rc = pthread_create (&p0, NULL, increment, name0);
  assert (rc == 0);
  rc = pthread_create (&p1, NULL, increment, name1);
  assert (rc == 0);
  pthread_join (p0, NULL);
  pthread_join (p1, NULL);
  printf ("current count = %d\n", count);
  return 0;
}