#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 5000

pid_t rc, rc1;

void
sign_handler (int signo)
{
  if (signo == SIGINT)
    {
      printf ("Parent received SIGINT. Sending signals to children...\n");
      kill (rc, 16);
      kill (rc1, 17);
    }
}

void
child_handler (int signo)
{
  if (signo == 16)
    {
      printf ("Child process 1 is killed by parent !!\n");
      exit (0);
    }
  else if (signo == 17)
    {
      printf ("Child process 2 is killed by parent !!\n");
      exit (0);
    }
}

int
main ()
{
  rc = fork ();
  if (rc < 0)
    {
      fprintf (stderr, "first fork fail");
      exit (1);
    }
  else if (rc == 0)
    // child process 1st;
    {
      signal (16, child_handler);
      pause ();
    }
  else if (rc > 0)
    {
      rc1 = fork ();
      if (rc1 < 0)
        {
          fprintf (stderr, "second fork fail");
          exit (1);
        }
      else if (rc1 == 0)
        // child process 2nd;
        {
          signal (17, child_handler);
          pause ();
        }
      else if (rc1 > 0)
        // parent process;
        {
          sleep (5);
          signal (SIGINT, sign_handler);
          printf ("Press Ctrl+C to simulate interrupt within 5 seconds...\n");
          sleep (5);
          wait (NULL);
          wait (NULL);
        }
    }
  return 0;
}