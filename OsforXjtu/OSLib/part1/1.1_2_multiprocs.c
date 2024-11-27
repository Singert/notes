#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int num = -1;

int
main ()
{
  pid_t rc = fork ();
  if (rc < 0)
    {
      fprintf (stderr, "fork fail");
      exit (1);
    }

  else if (rc == 0)
    {
      printf ("rc = %d \n", rc);
      printf ("child (pid = %d) begin & num = %d\n", getpid (), num);
      num = 1;
      printf ("child end & num = %d \n", num);
      system ("./system_call");
      char *args[3];
      args[0] = "wc";
      args[1] = "Makefile";
      args[2] = NULL;
      execvp (args[0], args);
      system ("./system_call");
    }

  else if (rc > 0)
    {
      wait (NULL);
      printf ("rc = %d \n", rc);
      printf ("parent (pid = %d) begin & num  = %d\n", getpid (), num);
      num = 0;
      printf ("parent end & num = %d\n", num);
    }

  printf ("current num = %d\n", num);
  num = -2;
  printf ("process end & num = %d\n", num);

  return 0;
}