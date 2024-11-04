#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

typedef struct lock_t
{
  int flag;
} lock_t;

void
init (lock_t *lock)
{
  lock->flag = 0;
}

void
lock (lock_t *lock)
{
  while (__sync_lock_test_and_set (&lock->flag, 1) == 1)
    {
      ; //
    }
}

void
unlock (lock_t *lock)
{
  lock->flag = 0;
}

int count = 0;
lock_t l;

void *
increment (void *arg)
{
  lock (&l);
  int curr;
  for (int i = 0; i < 100000; i++)
    {
      count++;
    }
  curr = count;
  unlock (&l);
  printf ("current count in %s is %d\n", (char *)arg, curr);
  return NULL;
}

int
main ()
{
  pthread_t p0, p1;
  char *tname[] = { "p0", "p1" };
  int rc;
  rc = pthread_create (&p0, NULL, increment, (void *)tname[0]);
  assert (rc == 0);

  rc = pthread_create (&p1, NULL, increment, (void *)tname[1]);
  assert (rc == 0);

  pthread_join (p0, NULL);
  pthread_join (p1, NULL);
  return 0;
}