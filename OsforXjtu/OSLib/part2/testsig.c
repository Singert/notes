#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

pid_t child_pid;

void child_handler(int signo) {
    printf("Child process %d received signal %d\n", getpid(), signo);
    exit(0);
}

void parent_handler(int signo) {
    printf("signo %d\n",signo);
    printf("Parent process received SIGINT, sending signal to child\n");
    kill(child_pid, SIGTERM);  // 向子进程发送 SIGTERM
}

int main() {
    signal(SIGINT, parent_handler);  // 父进程处理 SIGINT
    child_pid = fork();

    if (child_pid == 0) {
        // 子进程
        signal(SIGTERM, child_handler);  // 子进程处理 SIGTERM
        printf("Child process started, waiting for signal...\n");
        pause();  // 等待信号
    } else {
        // 父进程
        printf("Parent process started, press Ctrl+C to send SIGINT...\n");
        sleep(2);  // 模拟父进程等待
        kill(getpid(), SIGINT);  // 向自己发送 SIGINT
        wait(NULL);  // 等待子进程结束
    }

    return 0;
}
