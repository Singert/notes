#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_PROC_COUNT 10
#define PROC_N_LEN 6
#define MIN_SLICE 10
#define DEFAULT_MEM_SIZE 1024
#define DEFAULT_MEM_START 0
#define MA_FF 1
#define MA_BF 2
#define MA_WF 3

typedef struct free_block_t
{
  int size;
  int start_addr;
  struct free_block_t *next;
} free_block_t;

typedef struct allocated_block_t
{
  int pid;
  int size;
  int start_addr;
  char pname[PROC_N_LEN];
  struct allocated_block_t *next;
} allocated_block_t;

free_block_t *free_head = NULL;

allocated_block_t *alloca_head = NULL;

int mem_size = DEFAULT_MEM_SIZE;
int mem_algorithm = 1;     // default allocation algorithm is FF
int procs[MAX_PROC_COUNT]; // procs[i] = 1 or 0 ,refers wether pid i is used or
                           // not
int flag = 0;

// 合并两个已排序的链表
free_block_t *
mergeSB (free_block_t *list1, free_block_t *list2)
{
  if (!list1)
    return list2;
  if (!list2)
    return list1;

  if (list1->size < list2->size)
    {
      list1->next = mergeSB (list1->next, list2);
      return list1;
    }
  else
    {
      list2->next = mergeSB (list1, list2->next);
      return list2;
    }
}

free_block_t *
mergeBS (free_block_t *list1, free_block_t *list2)
{
  if (!list1)
    return list2;
  if (!list2)
    return list1;

  if (list1->size > list2->size)
    {
      list1->next = mergeBS (list1->next, list2);
      return list1;
    }
  else
    {
      list2->next = mergeBS (list1, list2->next);
      return list2;
    }
}

// 分割链表为两半
void
split (free_block_t *head, free_block_t **front, free_block_t **back)
{
  free_block_t *fast;
  free_block_t *slow;
  slow = head;
  fast = head->next;

  while (fast != NULL)
    {
      fast = fast->next;
      if (fast != NULL)
        {
          slow = slow->next;
          fast = fast->next;
        }
    }
  *front = head;
  *back = slow->next;
  slow->next = NULL;
}

// 归并排序
void
mergeSortSB (free_block_t **headRef)
{
  free_block_t *head = *headRef;
  free_block_t *a;
  free_block_t *b;

  if (head == NULL || head->next == NULL)
    {
      return;
    }

  split (head, &a, &b);
  mergeSortSB (&a);
  mergeSortSB (&b);

  *headRef = mergeSB (a, b);
}

// 归并排序
void
mergeSortBS (free_block_t **headRef)
{
  free_block_t *head = *headRef;
  free_block_t *a;
  free_block_t *b;

  if (head == NULL || head->next == NULL)
    {
      return;
    }

  split (head, &a, &b);
  mergeSortBS (&a);
  mergeSortBS (&b);

  *headRef = mergeBS (a, b);
}

void
init_mem (int size)
{
  mem_size = size;
  printf ("memory size %d set\n", size);
  free_head = (free_block_t *)malloc (sizeof (free_block_t));
  free_head->start_addr = 0;
  free_head->size = mem_size;
  free_head->next = NULL;
  for (int i = 0; i < MAX_PROC_COUNT; i++)
    {
      procs[i] = 0;
    }
}

void
init_mem_algo (int choice)
{
  if (choice > 3 || choice < 1)
    {
      printf ("current algorithm %d is not supported,try again\n", choice);
      exit (1);
    }
  mem_algorithm = choice;
  printf ("current memory allocation algorithm is ");
  switch (choice)
    {
    case 1:
      printf ("MA_FF\n");
      break;
    case 2:
      printf ("MA_BF\n");
      break;
    case 3:
      printf ("MA_WF\n");
      break;
    }
}

int
ma_FF (int pid, char *name, int size)
{
  int success = 0; //是否成功分配空间
  free_block_t *free_curr;
  free_curr = free_head;
  while (free_curr != NULL)
    {
      if (free_curr->size > size || free_curr->size == size)
        {

          allocated_block_t *alloca_temp
              = (allocated_block_t *)malloc (sizeof (allocated_block_t));
          allocated_block_t *alloca_curr
              = (allocated_block_t *)malloc (sizeof (allocated_block_t));
          alloca_curr = alloca_head;

          alloca_temp->pid = pid;
          strncpy (alloca_temp->pname, name, sizeof (alloca_temp->pname) - 1);
          alloca_temp->size = size;
          alloca_temp->start_addr = free_curr->start_addr;
          //创建的allocated_block插入分配链表中
          if (alloca_head == NULL)
            {
              alloca_head = alloca_temp;
            }
          else
            {
              while (alloca_curr->next != NULL)
                {
                  alloca_curr = alloca_curr->next;
                }
              alloca_curr->next = alloca_temp;
            }

          free_curr->size = free_curr->size - size;
          free_curr->start_addr
              = free_curr->start_addr + size; //修改 空间区链表
          if (free_curr->size == 0)
            {
              free_curr = free_curr->next;
            }

          success = 1;
          break;
        }
      else
        {
          free_curr = free_curr->next;
        }
    }

  return success;
}

int
ma_BF (int pid, char *name, int size)
{
  int success = 0; //是否成功分配空间
  free_block_t *free_curr = (free_block_t *)malloc (sizeof (free_block_t));
  free_block_t *free_temp = (free_block_t *)malloc (sizeof (free_block_t));
  int delta_size;

  free_curr = free_head;
  free_temp = free_head;
  delta_size = free_temp->size - size;

  while (free_curr != NULL)
    {
      int curr_delta_size = free_curr->size - size;
      if ((!(delta_size < 0)) && (!(curr_delta_size < 0))
          && (delta_size > curr_delta_size))
        {
          free_temp = free_curr;
          delta_size = curr_delta_size;
        }
      else
        {
          free_curr = free_curr->next;
        }
    }
  if (!(delta_size < 0))
    {
      allocated_block_t *alloca_temp
          = (allocated_block_t *)malloc (sizeof (allocated_block_t));
      allocated_block_t *alloca_curr
          = (allocated_block_t *)malloc (sizeof (allocated_block_t));
      alloca_curr = alloca_head;

      alloca_temp->pid = pid;
      strncpy (alloca_temp->pname, name, sizeof (alloca_temp->pname) - 1);
      alloca_temp->size = size;
      alloca_temp->start_addr = free_temp->start_addr;
      //创建的allocated_block插入分配链表中
      if (alloca_head == NULL)
        {
          alloca_head = alloca_temp;
        }
      else
        {
          while (alloca_curr->next != NULL)
            {
              alloca_curr = alloca_curr->next;
            }
          alloca_curr->next = alloca_temp;
        }

      free_temp->size = free_temp->size - size;
      free_temp->start_addr = free_temp->start_addr + size; //修改 空间区链表

      success = 1;
    }

  return success;
}

int
ma_WF (int pid, char *name, int size)
{
  int success = 0; //是否成功分配空间
  free_block_t *free_curr = (free_block_t *)malloc (sizeof (free_block_t));
  free_block_t *free_temp = (free_block_t *)malloc (sizeof (free_block_t));
  int temp_size;

  free_curr = free_head;
  free_temp = free_head;
  temp_size = free_temp->size;

  while (free_curr != NULL)
    {
      int curr_size = free_curr->size;
      if (temp_size < curr_size)
        {
          free_temp = free_curr;
          temp_size = curr_size;
        }
      else
        {
          free_curr = free_curr->next;
        }
    }
  if (!(temp_size - size < 0))
    {
      allocated_block_t *alloca_temp
          = (allocated_block_t *)malloc (sizeof (allocated_block_t));
      allocated_block_t *alloca_curr
          = (allocated_block_t *)malloc (sizeof (allocated_block_t));
      alloca_curr = alloca_head;

      alloca_temp->pid = pid;
      strncpy (alloca_temp->pname, name, sizeof (alloca_temp->pname) - 1);
      alloca_temp->size = size;
      alloca_temp->start_addr = free_temp->start_addr;
      //创建的allocated_block插入分配链表中
      if (alloca_head == NULL)
        {
          alloca_head = alloca_temp;
        }
      else
        {
          while (alloca_curr->next != NULL)
            {
              alloca_curr = alloca_curr->next;
            }
          alloca_curr->next = alloca_temp;
        }

      free_temp->size = free_temp->size - size;
      free_temp->start_addr = free_temp->start_addr + size; //修改 空间区链表

      success = 1;
    }

  return success;
}

void
generate_proc_name (char *name)
{
  for (int i = 0; i < 5; i++)
    {
      name[i] = rand () % 26 + 'A';
    }
  name[6] = '\0';
}

int
alloc_pid ()
{
  // 分配pid，查找当前尚未分配的pid；
  int pid = 0;
  while (procs[pid] != 0)
    {
      pid++;
    }
  printf ("alloc pid = %d\n", pid);
  return pid;
}

void
take_pid (int pid)
{
  //完成内存分配后，占有pid；
  procs[pid] = 1;
}

void
free_pid (int pid)
{
  procs[pid] = 0;
}

void
alloc_mem (int size)
{

  int pid = alloc_pid ();
  char *name = (char *)malloc (6 * sizeof (char));
  int alloca_success;
  printf ("[generate_proc_name] start\n");
  generate_proc_name (name);
  printf ("[generate_proc_name] success && proc name is %s\n", name);
  printf ("attempt to create new process pid [%d] name [%s]\n", pid, name);

  switch (mem_algorithm)
    {
    case MA_FF:
      printf ("[ma_FF] begin\n");
      alloca_success = ma_FF (pid, name, size);
      printf ("[ma_FF] success\n");
      break;
    case MA_BF:
      printf ("[ma_BF] begin\n");
      alloca_success = ma_BF (pid, name, size);
      printf ("[ma_BF] success\n");
      break;
    case MA_WF:
      printf ("[ma_WF] begin\n");
      alloca_success = ma_WF (pid, name, size);
      printf ("[ma_WF] success\n");
      break;
    }
  if (alloca_success == 0)
    {
      printf ("create fail \n");
    }
  else
    {
      take_pid (pid);
      printf ("create success \n");
    }
}

void
free_proc (int pid)
{

  // 删除块
  allocated_block_t *alloca_temp
      = (allocated_block_t *)malloc (sizeof (allocated_block_t));
  alloca_temp = alloca_head;
  free_block_t *free_to_add = (free_block_t *)malloc (sizeof (free_block_t));
  // free_block_t *free_temp = (free_block_t *)malloc (sizeof (free_block_t));
  free_block_t *free_curr = (free_block_t *)malloc (sizeof (free_block_t));
  // 寻找pid所对应的内存块,释放内存；
  if (alloca_temp->pid == pid)
    {

      free_to_add->size = alloca_temp->size;
      free_to_add->start_addr = alloca_temp->start_addr;
      alloca_head = alloca_temp->next;
    }
  else
    {

      while (alloca_temp->next->pid != pid)
        {
          alloca_temp = alloca_temp->next;
        }
      free_to_add->size = alloca_temp->size;
      free_to_add->start_addr = alloca_temp->start_addr;
      alloca_temp->next = alloca_temp->next->next;
    }
  // 将释放的内存块添加到空闲块列表中;
  if (free_to_add->start_addr < free_head->start_addr)
    {
      free_to_add->next = free_head;
      free_head = free_to_add;
      //合并相邻块；
      if (free_head->start_addr + free_head->size
          == free_head->next->start_addr)
        {
          free_head->size = free_head->next->size + free_head->size;
          free_head->next = free_head->next->next;
        }
    }
  else
    {

      free_curr = free_head;

      while (free_curr != NULL)
        {
          if (free_curr->start_addr < free_to_add->start_addr
              && free_curr->next->start_addr > free_to_add->start_addr)

            {
              free_to_add->next = free_curr->next;
              free_curr->next = free_to_add;
              //合并相邻块；
              if (free_curr->start_addr + free_curr->size
                  == free_curr->next->start_addr)
                {
                  free_curr->size += free_curr->next->size;
                  free_curr->next = free_curr->next->next;
                  if (free_curr->start_addr + free_curr->size
                      == free_curr->next->start_addr)
                    {
                      free_curr->size += free_curr->next->size;
                      free_curr->next = free_curr->next->next;
                    }
                }
              else if (free_to_add->start_addr + free_to_add->size
                       == free_to_add->next->start_addr)
                {
                  free_to_add->size += free_to_add->next->size;
                  free_to_add->next = free_to_add->next->next;
                }
              break;
            }
          else if (free_curr->start_addr < free_to_add->start_addr
                   && free_curr->next == NULL)
            {
              free_to_add->next = free_curr->next;
              free_curr->next = free_to_add;
              // 合并相邻块；
              if (free_curr->start_addr + free_curr->size
                  == free_curr->next->start_addr)
                {
                  free_curr->size += free_curr->next->size;
                  free_curr->next = NULL;
                }
              break;
            }
          else
            {
              free_curr = free_curr->next;
            }
        }
    }
}

void
ra_BF ()
{
  mergeSortSB (&free_head);
}

void
ra_WF ()
{
  mergeSortBS (&free_head);
}

void
free_mem (int pid)
{
  printf ("[free_proc] begin\n");
  free_proc (pid);
  printf ("[free_proc] success\n");
  switch (mem_algorithm)
    {
    case MA_BF:
      printf ("[ra_BF] begin\n");
      ra_BF ();
      printf ("[ra_BF] success\n");
      break;
    case MA_WF:
      printf ("[ra_wF] begin\n");
      ra_WF ();
      printf ("[ra_WF] success\n");
      break;
    }
}

void
show_free()
{
  printf("|————————————————————|\n");
  free_block_t* free_curr;
  free_curr = free_head;
  while(free_curr != NULL)
  {
    printf(" start_addr:%d\n",free_curr->start_addr);
    printf(" size:%d\n",free_curr->size);
    printf("|————————————————————|\n");
    free_curr = free_curr->next;
  }
  

}

void
show_alloc()
{
  printf("|————————————————————|\n");
  allocated_block_t* alloc_curr;
  alloc_curr = alloca_head;
  while(alloc_curr != NULL)
  {
    printf(" start_addr:%d\n",alloc_curr->start_addr);
    printf(" size:%d\n",alloc_curr->size);
    printf(" pid:%d\n",alloc_curr->pid);
    printf(" proc_name:%s\n",alloc_curr->pname);
    printf("|————————————————————|\n");
    alloc_curr = alloc_curr->next;
  }
  

}


void 
show(int signal)
{
  switch (signal) {
  case 1:
    printf("[show_free] begin \n");
    show_free();
    printf("[show_begin] success \n");
    break;
  case 2:
    printf("[show_alloc] begin \n");
    show_alloc();
    printf("[show_alloc] success \n");
    break;
  }
}

void
help ()
{
  printf ("Usage: ./memory [OPTION],[AGRUMENT] [OPTION],[AGRUMENT]... \n"
          " -1      set memory size (default=1024)\n"
          " -2      select memory allocation algorithm\n"
          " -3      new process,support creating no more than 10 processes\n"
          " -4      terminate a process\n"
          " -5      display memory usage\n"
          " -0      exit\n");
}

// TODO: parse command
/*
1 - Set memory size (default=1024)
2 - Select memory allocation algorithm
3 - New process
4 - Terminate a process
5 - Display memory usage
0 - Exit
*/
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
  switch (cmd[0])
    {
    case 1:
      printf ("[init_mem] begin\n");
      init_mem (cmd[1]);
      printf ("[init_mem] success\n");
      break;

    case 2:
      printf ("[init_mem_algo] begin\n");
      init_mem_algo (cmd[1]);
      printf ("[init_mem] success\n");

      break;

    case 3:
      printf ("[alloc_mem] begin\n");
      alloc_mem (cmd[1]);
      printf ("[alloc_mem] success\n");
      break;

    case 4:
      printf ("command free_mem for process pid = %d is received \n", cmd[1]);
      printf ("[free_mem] begin\n");
      free_mem (cmd[1]);
      printf ("[free_mem] success\n");
      break;
    case 5:
      // TODO
      printf ("case 5");
      show(cmd[1]);
      break;
    }
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