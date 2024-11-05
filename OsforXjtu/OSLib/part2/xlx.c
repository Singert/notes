
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
int flag = 0;
pid_t pid1 = -1, pid2 = -1;

void inter_handler(int sig) {
  // TODO
  if (pid1 > 0 && pid2 > 0) {
    if (sig == SIGALRM || sig == SIGINT || sig == SIGQUIT) {
      // 调用kill()向两个子进程发中断信号
      printf("sig:%dinterrupt\n", sig);
      kill(pid2, 17);
      kill(pid1, 16);
    }
  }
  if (pid1 == 0) {
    if (sig == SIGQUIT || sig == SIGINT) {
    }
    if (sig == 16) {
      flag = 1;
      printf("sig:%dinterrupt\n", sig);
    }
  }
  if (pid2 == 0) {
    if (sig == SIGQUIT || sig == SIGINT) {
    }
    if (sig == 17) {
      flag = 1;
      printf("sig:%dinterrupt\n", sig);
    }
  }

  return;
}
void waiting() {
  // TODO
  flag = 2;
  printf("waiting\n");
  return;
}
int main() {
  // TODO: 五秒之后或接收到两个信号
  signal(SIGALRM, inter_handler);
  signal(SIGQUIT, inter_handler);
  signal(SIGINT, inter_handler);
  while (pid1 == -1)
    pid1 = fork();
  if (pid1 > 0) {
    while (pid2 == -1)
      pid2 = fork();
    if (pid2 > 0) {
      // TODO: 父进程
      printf("parent process\n");
      // 等待软中断发生
      // 等待5秒后，如果软中断发生，接受软中断SIGALRM,否则接受软中断SIGQUIT
      alarm(5);
      // 等待子进程终止后父进程终止
      wait(NULL);
      printf("\nParent process is killed!!\n");
    } else {
      // TODO: 子进程 2
      printf("child process2\n");
      // 子进程2等待软中断信号17发生
      signal(17, inter_handler);
      while (flag == 0) {
        sleep(1);
      }
      // 如果得到信号17则子进程2终止
      printf("\nChild process2 is killed by parent!!\n");
      return 0;
    }
  } else {
    // TODO:子进程 1
    printf("child process1\n");
    // 等待软中断信号16发生
    signal(16, inter_handler);
    while (flag == 0) {
      sleep(1);
    }
    // 如果得到信号16则子进程1终止
    printf("\nChild process1 is killed by parent!!\n");
    return 0;
  }
  return 0;
}