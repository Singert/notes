#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define FIFO 1
#define LRU 2
typedef struct page_t
{
  int vpn;
  int length;
  struct page_t *next;
} page_t;

typedef struct cache_t
{
  page_t *head;
  page_t *tail;
} cache_t;

void
init_cache (cache_t *cache)
{
  cache->head = cache->tail = NULL;
  return;
}

cache_t *cache;
int max_page_count = 3; // default page count = 3;
int mmu_algo = 1;       // default algo is FIFO;

void
init_page_count (int count)
{
  max_page_count = count;
}

void
init_mmu_algo (int choice)
{
  mmu_algo = choice;
}

int
fifu (int pn)
{
  int signal = 1; // signal = 1表示未命中，0表示命中；
  page_t *page_to_add = (page_t *)malloc (sizeof (page_t));
  page_to_add->vpn = pn;
  page_to_add->next = NULL;
  if (cache->head == cache->tail && cache->tail == NULL)
    {
      page_to_add->length = 1;
      cache->head = page_to_add;
      cache->tail = page_to_add;
    }
  else
    {
      page_t *page_temp = cache->head;
      while (page_temp != NULL)
        {
          if (page_temp->vpn == pn)
            {
              signal = 0; //命中
              break;
            }
          else
            {
              page_temp = page_temp->next;
            }
        }
      if (signal == 1)
        {
          if (cache->tail->length > 0 && cache->tail->length < max_page_count)
            {
              page_to_add->length = cache->tail->length + 1;
              cache->tail->next = page_to_add;
              cache->tail = cache->tail->next;
            }
          else if (cache->tail->length == max_page_count)
            {
              page_to_add->length = max_page_count + 1;
              cache->head = cache->head->next;
              cache->tail->next = page_to_add;
              cache->tail = cache->tail->next;
              page_temp = cache->head;
              while (page_temp != NULL)
                {
                  page_temp->length = page_temp->length - 1;
                  page_temp = page_temp->next;
                }
            }
        }
    }

  return signal;
}

int
lru (int pn)
{
  int signal = 1; // signal = 1表示未命中，0表示命中；
  page_t *page_to_add = (page_t *)malloc (sizeof (page_t));
  page_to_add->vpn = pn;
  page_to_add->next = NULL;
  if (cache->head == cache->tail && cache->tail == NULL)
    {
      page_to_add->length = 1;
      cache->head = page_to_add;
      cache->tail = page_to_add;
    }
  else
    {
      page_t *page_hit = NULL;
      page_t *page_before_hit = NULL;
      page_t *page_temp = cache->head;
      if (page_temp->vpn == pn)
        {
          page_hit = page_temp;
          signal = 0; //命中；
        }
      while (signal == 1 && page_temp->next != NULL)
        {
          if (page_temp->next->vpn == pn)
            {
              signal = 0; //命中
              page_before_hit = page_temp;
              break;
            }
          else
            {
              page_temp = page_temp->next;
            }
        }
      if (signal == 1)
        {
          if (cache->tail->length > 0 && cache->tail->length < max_page_count)
            {
              page_to_add->length = cache->tail->length + 1;
              cache->tail->next = page_to_add;
              cache->tail = cache->tail->next;
            }
          else if (cache->tail->length == max_page_count)
            {
              page_to_add->length = max_page_count + 1;
              cache->head = cache->head->next;
              cache->tail->next = page_to_add;
              cache->tail = cache->tail->next;
              page_temp = cache->head;
              while (page_temp != NULL)
                {
                  page_temp->length = page_temp->length - 1;
                  page_temp = page_temp->next;
                }
            }
        }
      else if (signal == 0)
        {
          //处理命中情况；
          if (page_hit != NULL && page_before_hit == NULL)
            {
              cache->head = cache->head->next;
              cache->tail->next = page_hit;
              cache->tail = cache->tail->next;
              page_temp = cache->head;

              int i = 1;
              while (page_temp != NULL)
                {
                  page_temp->length = i;
                  i++;
                  page_temp = page_temp->next;
                }
            }
          else if (page_hit == NULL && page_before_hit != NULL)
            {
              page_hit = page_before_hit->next;
              if (page_hit != cache->tail)
                {
                  page_before_hit->next = page_before_hit->next->next;
                  cache->tail->next = page_hit;
                  cache->tail = cache->tail->next;
                  int i = 1;
                  page_temp = cache->head;
                  while (page_temp != NULL)
                    {
                      page_temp->length = i;
                      i++;
                      page_temp = page_temp->next;
                    }
                }
            }
        }
    }

  return signal;
}

void
access_page (int nums)

{
  switch (mmu_algo)
    {
    case FIFO:
      fifu ();
      break;
    case LRU:
      lru ();
      break;
    }
}

void
help ()
{
  printf ("Usage: ./page [OPTION],[AGRUMENT] [OPTION],[AGRUMENT]... \n"
          " -1      set MAX_PAGE_COUNT\n"
          " -2      select memory allocation algorithm\n"
          " -3      set page access policy,-1 means random access,"
          " -0      exit\n");
}

void
parse_command (char *input)
{
  int cmd[2];
  int i = 0;
  char *token = strtok (input, " ");
  while (token != NULL)
    {
      printf ("cmd[%d] : %s is recived\n", i, token);
      cmd[i] = atoi (token);
      i++;
      token = strtok (NULL, " ");
    }
  init_cache (cache);
  switch (cmd[0])
    {
    case 1:
      printf ("[init_page_count] begin\n");
      init_page_count (cmd[1]);
      printf ("[init_page_count] success\n");
      break;

    case 2:
      printf ("[init_mmu_algo] begin\n");
      init_mmu_algo (cmd[1]);
      printf ("[init_mmu_algo] success\n");

      break;

    case 3:
      printf ("[access] begin\n");
      access_page (cmd[1]);
      printf ("[access] success\n");
      break;
    }
  return;
}

int
main ()
{
  help ();
  while (1)
    {
      char input[100];
      printf ("输入: ");
      fgets (input, sizeof (input), stdin);

      // 去掉行末的换行符
      input[strcspn (input, "\n")] = 0;

      // 检查结束条件
      if (strcmp (input, "0") == 0)
        {
          printf ("quit \n");
          break;
        }
      parse_command (input);
    }

  return 0;
}