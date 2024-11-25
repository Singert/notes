#include "ext2_fs_sb.h"
#include "ext2_fs.h"
#include "ext2_fs_i.h"
#include "include/common.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define BLK_SIZE 4096            //块大小为4KB
#define UNIVERSAL_GRP_BLKS 16384 //每个块组块数:16384
#define UNIVERSAL_GRP_SIZE                                                    \
  (BLK_SIZE * UNIVERSAL_GRP_BLKS) //每个块组的大小（byte）
#define IMG_PATH "../part3/files/test.img"

#define blk_ofst blk_size               //块偏移量
#define blk_grp_ofst universal_grp_size //块组偏移量
#define MIN_GRP_SIZE

int incomplete;
struct ext2_super_block_t common_spr_blk;
struct ext2_inode_t common_ind;
int img_fd;
long img_size;    // img文件大小（byte）
int group_number; // img所能容纳的块组个数；

//获取img的文件描述符;
int
init_fd ()
{
  img_fd = open (IMG_PATH, O_RDWR);
  if (img_fd == -1)
    {
      fprintf (stderr, "open img failed");
      return -1;
    }
  // TODO:do something;
  return 0;
}

// 获取img文件大小;
int
get_img_size ()
{
  init_fd ();
  struct stat img_stat;
  if (fstat (img_fd, &img_stat) != 0)
    {
      fprintf (stderr, "unable to get file stats");
      return -1;
      exit (EXIT_FAILURE);
    }
  close (img_fd);
  img_size = img_stat.st_size;
  return 0;
}
// 写入supperblk;
void
init_spr_blk ()
{
}

//写入inode
void
init_inode ()
{
}

void
init_blk_grp ()
{
  for (int i = 0; i < group_number; i++)
    {
    }
}

//格式化磁盘映像文件；
void
init_file_system ()
{
  // 获取文件大小
  if (get_img_size () < 0)
    {
      fprintf (stderr, "get img file size error");
      exit (EXIT_FAILURE);
    }
  //计算块组数量
  group_number = (int)img_size / UNIVERSAL_GRP_SIZE;
  if (img_size % UNIVERSAL_GRP_SIZE > 278528)
    {
      incomplete = 1;
      group_number += 1;
    }
  else
    {
      incomplete = 0;
    }
  //按组写入img;
}

int
main ()
{

  return 0;
}
