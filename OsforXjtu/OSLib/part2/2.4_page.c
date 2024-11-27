#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

typedef struct
{
  int *digits; // 用于存储结果列表
  int size;    // 列表元素个数
} access_t;

cache_t *cache;
int max_page_count = 3; // default page count = 3;
int mmu_algo = 1;       // default algo is FIFO;

//生成六位以上随机数
int
generate_random_int ()
{
  // 初始化随机数种子
  srand ((unsigned)time (NULL));

  // 生成六位或以上的随机数
  int randomInt = rand () % 900000 + 100000; // 保证至少六位
  return randomInt;
}

access_t
extract_digits (int number)
{
  int temp = number;
  int size = 0;

  // 计算位数
  do
    {
      size++;
      temp /= 10;
    }
  while (temp != 0);

  // 分配存储位的数组
  int *digits = (int *)malloc (size * sizeof (int));
  if (digits == NULL)
    {
      perror ("Memory allocation failed");
      exit (EXIT_FAILURE);
    }

  // 将数字从高位到低位存入数组
  temp = number;
  for (int i = size - 1; i >= 0; i--)
    {
      digits[i] = temp % 10;
      temp /= 10;
    }

  access_t access_t = { digits, size };
  return access_t;
}

void
print_cache (const cache_t *cache)
{
  if (cache == NULL || cache->head == NULL)
    {
      printf ("Cache is empty.\n");
      return;
    }

  page_t *current = cache->head;
  while (current != NULL)
    {
      printf ("%d ", current->vpn);
      current = current->next;
    }
  printf (" \n");
}

void
init_cache ()
{
  cache = (cache_t *)malloc (sizeof (cache_t));
  cache->head = cache->tail = NULL;
  return;
}

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
  int signal = 0; // signal = 0表示未命中，1表示命中；
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
              signal = 1; //命中
              break;
            }
          else
            {
              page_temp = page_temp->next;
            }
        }
      if (signal == 0)
        {
          //缓存队列未满情况
          if (cache->tail->length > 0 && cache->tail->length < max_page_count)
            {
              page_to_add->length = cache->tail->length + 1;
              cache->tail->next = page_to_add;
              cache->tail = cache->tail->next;
              printf ("[踢出]:%d  ", -1);
            }
          //缓存队列已满情况
          else if (cache->tail->length == max_page_count)
            {
              page_to_add->length = max_page_count + 1;
              printf ("[踢出]: %d  ", cache->head->vpn);
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
      else if (signal == 1)
        {
          printf ("[踢出]:%d  ", -1);
        }
    }

  return signal;
}

// int
// lru (int pn)
// {
//   int signal = 0; // signal = 0表示未命中，1表示命中；
//   page_t *page_to_add = (page_t *)malloc (sizeof (page_t));
//   page_to_add->vpn = pn;
//   page_to_add->next = NULL;
//   if (cache->head == cache->tail && cache->tail == NULL)
//     {
//       page_to_add->length = 1;
//       cache->head = page_to_add;
//       cache->tail = page_to_add;
//     }
//   else
//     {
//       page_t *page_hit = NULL;
//       page_t *page_before_hit = NULL;
//       page_t *page_temp = cache->head;
//       if (page_temp->vpn == pn)
//         {
//           page_hit = page_temp;
//           signal = 1; //命中；
//         }
//       while (signal == 0 && page_temp->next != NULL)
//         {
//           // printf ("all1\n");
//           if (page_temp->next->vpn == pn)
//             {
//               // printf ("if1\n");
//               signal = 1; //命中
//               page_before_hit = page_temp;
//               // printf ("if2\n");

//               break;
//             }
//           else
//             {
//               // printf ("else1\n");
//               page_temp = page_temp->next;
//               // printf ("%d\n", page_temp->vpn);
//               // printf ("else2\n");
//             }
//           // printf ("all2\n");
//         }
//       if (signal == 0)
//         {
//           //缓存队列未满情况
//           if (cache->tail->length > 0 && cache->tail->length <
//           max_page_count)
//             {
//               page_to_add->length = cache->tail->length + 1;
//               cache->tail->next = page_to_add;
//               cache->tail = cache->tail->next;
//               printf ("[踢出]:%d  ", -1);
//             }
//           //缓存队列已满情况
//           else if (cache->tail->length == max_page_count)
//             {
//               page_to_add->length = max_page_count + 1;
//               cache->head = cache->head->next;
//               cache->tail->next = page_to_add;
//               printf ("[踢出]: %d  ", cache->tail->vpn);
//               cache->tail = cache->tail->next;
//               page_temp = cache->head;
//               while (page_temp != NULL)
//                 {
//                   page_temp->length = page_temp->length - 1;
//                   page_temp = page_temp->next;
//                 }
//             }
//         }
//       else if (signal == 1)
//         {
//           //处理命中情况；
//           printf ("[踢出]:%d  ", -1);
//           if (page_hit != NULL && page_before_hit == NULL)
//             {
//               cache->head = cache->head->next;
//               cache->tail->next = page_hit;
//               cache->tail = cache->tail->next;
//               page_temp = cache->head;

//               int i = 1;
//               while (page_temp != NULL)
//                 {
//                   page_temp->length = i;
//                   i++;
//                   page_temp = page_temp->next;
//                 }
//             }
//           else if (page_hit == NULL && page_before_hit != NULL)
//             {
//               page_hit = page_before_hit->next;
//               if (page_hit != cache->tail)
//                 {
//                   page_before_hit->next = page_before_hit->next->next;
//                   cache->tail->next = page_hit;
//                   cache->tail = cache->tail->next;
//                   int i = 1;
//                   page_temp = cache->head;
//                   while (page_temp != NULL)
//                     {
//                       page_temp->length = i;
//                       i++;
//                       page_temp = page_temp->next;
//                     }
//                 }
//             }
//         }
//     }

//   return signal;
// }

// 获取缓存中页面的数量
int
get_cache_size ()
{
  int size = 0;
  page_t *page_temp = cache->head;
  while (page_temp != NULL)
    {
      size++;
      page_temp = page_temp->next;
    }
  return size;
}

int
lru (int pn)
{
  int signal = 0; // signal = 0表示未命中，1表示命中；
  page_t *page_to_add = (page_t *)malloc (sizeof (page_t));
  page_to_add->vpn = pn;
  page_to_add->next = NULL;

  if (cache->head == NULL)
    {
      // 如果缓存为空，直接将新页面添加到缓存
      printf ("[踢出]: -1 ");
      page_to_add->length = 1;
      cache->head = page_to_add;
      cache->tail = page_to_add;
    }
  else
    {
      page_t *page_temp = cache->head;
      page_t *page_hit = NULL;
      page_t *page_before_hit = NULL;

      // 遍历缓存查找命中页面
      while (page_temp != NULL)
        {
          if (page_temp->vpn == pn)
            {
              // 如果命中，记录命中页面及其前一个页面
              page_hit = page_temp;
              signal = 1;
              break;
            }
          page_before_hit = page_temp;
          page_temp = page_temp->next;
        }

      if (signal == 0)
        {
          // 如果未命中，处理替换和插入新页面
          if (cache->tail->length < max_page_count)
            {
              // 缓存未满，直接添加到队尾
              page_to_add->length = cache->tail->length + 1;
              cache->tail->next = page_to_add;
              cache->tail = page_to_add;
              printf ("[踢出]: -1 ");
            }
          else
            {
              // 缓存已满，删除头部页面，并插入新页面到队尾
              printf ("[踢出]: %d ", cache->head->vpn);
              cache->head = cache->head->next;
              cache->tail->next = page_to_add;
              cache->tail = page_to_add;
              page_temp = cache->head;
              while (page_temp != NULL)
                {
                  page_temp->length = page_temp->length - 1;
                  page_temp = page_temp->next;
                }
            }
        }
      else
        {
          // 处理命中情况，将命中的页面移到队尾
          if (page_hit != cache->tail)
            {
              // 如果命中不是尾部页面，先移除该页面
              if (page_before_hit != NULL)
                {
                  page_before_hit->next = page_hit->next;
                }
              if (page_hit == cache->head)
                {
                  cache->head = page_hit->next;
                }

              // 将页面移到队尾
              cache->tail->next = page_hit;
              cache->tail = page_hit;
              page_hit->next = NULL;
            }
          printf ("[踢出]: -1 ");
        }
    }

  return signal;
}

void
access_page (int nums)

{
  printf ("[extract_digits]: begin\n");
  access_t access = extract_digits (nums);
  printf ("[extract_digits]: done\n");
  int sum;
  double hitRate;
  switch (mmu_algo)
    {
    case FIFO:
      sum = 0;
      for (int i = 0; i < access.size; i++)
        {
          printf ("[access]: %d  ", access.digits[i]);
          int sign = fifu (access.digits[i]);
          printf ("[命中]: %d  ", sign);
          printf ("[缓存状态]:");
          print_cache (cache);
          sum += sign;
        }
      hitRate = (double)sum / access.size * 100.0;
      printf ("[缓存命中率]: %.2f%%\n", hitRate);
      break;
    case LRU:
      sum = 0;
      for (int i = 0; i < access.size; i++)
        {
          printf ("%d", access.digits[i]);
        }
      for (int i = 0; i < access.size; i++)
        {
          printf ("i\n");
          printf ("[access]: %d  ", access.digits[i]);
          int sign = lru (access.digits[i]);
          printf ("[命中]: %d  ", sign);
          printf ("[缓存状态]:");
          print_cache (cache);
          sum += sign;
          printf ("ia\n");
        }
      hitRate = (double)sum / access.size * 100.0;
      printf ("[缓存命中率]: %.2f%% \n", hitRate);
      break;
    }
}

void
help ()
{
  printf ("Usage: ./page [OPTION],[AGRUMENT] [OPTION],[AGRUMENT]... \n"
          " 1      set MAX_PAGE_COUNT\n"
          " 2      select memory allocation algorithm\n"
          "        ..........argument: 1:FIFO,2:LRU 2\n"
          " 3      set page access policy,-1 means random access\n"
          " 0      exit\n");
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
  printf ("[init_cache]: begin\n");
  init_cache ();
  printf ("[init_cache]: done\n");

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
      if (cmd[1] == -1)
        {
          printf ("[generate_random_int]: begin\n");
          cmd[1] = generate_random_int ();
          printf ("[generate_random_int]: done\n");
        }
      printf ("[access_page]: begin\n");
      access_page (cmd[1]);
      printf ("[access_page]: done\n");
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