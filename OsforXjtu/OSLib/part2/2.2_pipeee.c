#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

pid_t rc1, rc2;

int
main ()
{
  char buffer[BUFFER_SIZE];
  int fd[2];
  int pipe_temp = pipe (fd);
  assert (pipe_temp == 0);

  rc1 = fork ();
  if (rc1 < 0)
    {
      fprintf (stderr, "fork child process 1 fail\n");
      exit (1);
    }
  else if (rc1 == 0)
    {
      // child process 1;
      // lockf (fd[1], 1, 0);
      for (int i = 0; i < 2000; i++)
        {
          dprintf (fd[1], "1");
        }
      // lockf (fd[1], 0, 0);
      exit (0);
    }
  else if (rc1 > 0)
    {
      rc2 = fork ();
      if (rc2 < 0)
        {
          fprintf (stderr, "fork child process 2 failed\n");
          exit (1);
        }
      else if (rc2 == 0)
        {
          // child process 2;
          lockf (fd[1], 1, 0);
          for (int i = 0; i < 2000; i++)
            {
              dprintf (fd[1], "2");
            }
          lockf (fd[1], 0, 0);
          exit (0);
        }
      else if (rc2 > 0)
        {
          // parent process;
          printf ("close child process %d\n", wait (NULL));
          printf ("close child process %d\n", wait (NULL));
          printf ("receive starting...\n");
          read (fd[0], buffer, BUFFER_SIZE);
          printf ("receive done: %s", buffer);
        }
    }
  return 0;
}