// #include <sched.h>
// #include <signal.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/wait.h>
// #include <unistd.h>

// int flag1 = 0;
// int flag2 = 0;
// pid_t rc, rc1;

// void
// sign_handler (int signo)
// {
//   // if (signo == SIGINT)
//   //   {
//   if (signo == SIGINT)
//   {

//   }
//       printf ("Parent received SIGINT. Sending signals to children...\n");
//       kill (rc, 16);
//       kill (rc1, 17);
//       printf("rc = %d & rc1 = %d\n",rc,rc1);
//     // }
// }

// void
// child_handler (int signo)
// {
//   printf("aaa\n");
//   if (signo == 16)
//     {
//       printf ("Child process 1 is killed by parent !!\n");
//       exit (0);
//     }
//   else if (signo == 17)
//     {
//       printf ("Child process 2 is killed by parent !!\n");
//       exit (0);
//     }
// }

// int
// main ()
// {
//   rc = fork ();
//   if (rc < 0)
//     {
//       fprintf (stderr, "first fork fail");
//       exit (1);
//     }
//   else if (rc == 0)
//     // child process 1st;
//     {
//       printf("A1 %d\n",getpid());
//       signal (16, child_handler);
//       printf("B1\n");
//       pause ();
//     }
//   else if (rc > 0)
//     {
//       rc1 = fork ();
//       if (rc1 < 0)
//         {
//           fprintf (stderr, "second fork fail");
//           exit (1);
//         }
//       else if (rc1 == 0)
//         // child process 2nd;
//         {
//           printf("A2 %d\n",getpid());
//           signal (17, child_handler);
//           printf("B2\n");
//           pause ();
//           exit(0);
//         }
//       else if (rc1 > 0)
//         // parent process;
//         {
//           printf("parent pid = %d \n",getpid());
//           sleep (2);
//           signal (SIGINT, sign_handler);
//           printf ("Press Ctrl+C to simulate interrupt within 5
//           seconds...\n"); sleep (5); wait (NULL); wait (NULL);
//         }
//     }
//   return 0;
// }

#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define SIGEND1 16
#define SIGEND2 17

pid_t rc1, rc2;

void
parent_handler (int signo)
{
  printf ("\n[p]parent process recived sig : %d\n", signo);
  if (kill (rc1, 16) < 0)
    {
      printf ("[p]failed to kill\n");
    }
  else
    {
      printf ("[p]process %d SIG %d send to child process %d\n", getpid (), 16,
              rc1);
    }
  if (kill (rc2, 17) < 0)
    {
      printf ("[p]failed to kill\n");
    }
  else
    {
      printf ("[p]process %d SIG %d send to child process %d\n", getpid (), 17,
              rc2);
    }
  printf ("[p]child %d waited\n", wait (NULL));
  printf ("[p]child %d waited\n", wait (NULL));
}

void
child_handler (int signo)
{
  printf ("[c]signal=%d\n", signo);
  if (signo == 16)
    {
      printf ("[c1]child process 1 (pid = %d ) is killed by signal %d\n",
              getpid (), signo);
    }
  if (signo == 17)
    {
      printf ("[c2]child process 2 (pid = %d ) is killed by signal %d\n",
              getpid (), signo);
    }
  exit (0);
}
void
void_handler (int signo)
{
  printf ("void handler: %d\n", signo);
}
int
main ()
{
  signal (SIGINT, void_handler);
  rc1 = fork ();
  if (rc1 < 0)
    {
      fprintf (stderr, "fork failed\n");
      exit (1);
    }
  else if (rc1 == 0)
    {
      printf ("[c1]current process : %d\n", getpid ());
      if (signal (16, child_handler) == SIG_ERR)
        {
          printf ("[c1]failed to set signal\n");
        }
      else
        {
          printf ("[c1]succeed to set signal, listening to %d\n", 16);
          //   kill (getpid (), 16);
        }
      // pause ();
      while (true)
        sleep (1);
      printf ("[c1]end of child process : %d\n", getpid ());
    }
  else if (rc1 > 0)
    {
      rc2 = fork ();
      if (rc2 < 0)
        {
          fprintf (stderr, "fork failed\n");
          exit (1);
        }
      else if (rc2 == 0)
        {
          printf ("[c2]current process : %d\n", getpid ());
          if (signal (17, child_handler) == SIG_ERR)
            {
              printf ("[c2]failed to set signal\n");
            }
          else
            {
              printf ("[c2]succeed to set signal, listening to %d\n", 17);
              // kill (getpid (), 17);
            }
          // pause ();
          while (true)
            sleep (1);
          printf ("[c2]end of child process : %d\n", getpid ());
        }
      else if (rc2 > 0)
        {
          if (signal (SIGINT, parent_handler) == SIG_ERR)
            {
              printf ("[p]failed to set signal\n");
            }
          else
            {
              printf ("[p]succeed to set signal\n");
            }
          printf ("[p]parent process (pid=%d) now waiting sigal:%d\n",
                  getpid (), SIGINT);
          sleep (10000);
          printf ("[p]end of the function\n");
        }
    }
  return 0;
}
