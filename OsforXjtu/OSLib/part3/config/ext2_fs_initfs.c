#include "ext2_fs.h"
#include "ext2_fs_i.h"
#include "ext2_fs_sb.h"
#include "include/common.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

struct ext2_super_block_t common_spr_blk;
struct ext2_inode_t common_ind;
struct ext2_group_desc_t common_grp_dsc;
int img_fd;
long img_size;    // img文件大小（byte）
int group_number; // img所能容纳的块组个数；
block_bit_map_t common_blk_btmp;
inode_bit_map_t common_ind_btmp;
inode_bit_map_t common_ind_btmp0;

// 写入数据到指定块
void
write_blocks (int fd, const void *data, size_t size, int block_index,
              int group_index)
{
  off_t offset = (group_index * UNIVERSAL_GRP_BLKS + block_index) * BLK_SIZE;
  if (lseek (fd, offset, SEEK_SET) == -1)
    {
      perror ("lseek failed");
      exit (EXIT_FAILURE);
    }
  if (write (fd, data, size) != (ssize_t)size)
    {
      perror ("write failed");
      exit (EXIT_FAILURE);
    }
}

//获取img的文件描述符;
int
init_fd ()
{
  int img_fd = open (IMG_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644);

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

// supperblk;
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

//创建一个通用的super_block
void
creat_common_spr_blk (_u32 group_index)
{
  common_spr_blk.s_inodes_count
      = 2048 * group_number; // FIXME: 文件系统中索引节点总数；
  common_spr_blk.s_blocks_count = 16384 * group_number; //文件系统中中块数
  common_spr_blk.s_free_blocks_count
      = 16316 * group_number; //文件系统中空闲块总数；
  common_spr_blk.s_free_inodes_count
      = 2048 * group_number; //文件系统中空闲索引节点总数
  common_spr_blk.s_first_data_block = 68; // 文件系统中第一个数据块的块号
  common_spr_blk.s_log_block_size
      = 3; //用于计算逻辑块大小（1--1024,2--2048 3--4096,4--8192；
  common_spr_blk.s_blocks_per_group = 16384; //每组中块数；
  common_spr_blk.s_inodes_per_group = 2048;  //每组中索引节点总数;
  common_spr_blk.s_mtime = (_u32)time (NULL); //最后一次安装操作的时间;
  common_spr_blk.s_wtime
      = (_u32)time (NULL); //最后一次对超级块进行写操作的时间;
  common_spr_blk.s_first_ino = 0;    //第一个非保留的索引节点；
  common_spr_blk.s_inode_size = 128; //索引节点的大小；
  common_spr_blk.s_block_group_nr = group_index; //该超级块的块组号；

  // performance hints.Directory preallocation should only
  //  happen if the EXT2_COMPAT_PREALLOC flag is on.
  common_spr_blk.s_prealloc_blocks = PRE_ALLOC_FILE_BLOCKS; //预分配的块数；
  common_spr_blk.s_prealloc_dir_blocks
      = PRE_ALLOC_DIR_BLOCKS; //给目录预分配的块数；
}

//创建通用的组描述符
void
creat_common_group_desc ()
{
  common_grp_dsc.bg_block_bitmap = 2; //组中块位图所在的块号
  common_grp_dsc.bg_inode_bitmap = 3; //族中索引节点位图所在的块号;
  common_grp_dsc.bg_inode_table = 4;  //族中索引节点表的首块号;
  common_grp_dsc.bg_free_blocks_count = 16316; //组中空闲块数;
  common_grp_dsc.bg_free_inodes_count = 2048;  //组中空闲索引节点数;
  common_grp_dsc.bg_used_dirs_count = 0; //组中分配给目录的节点数;
                                         //填充对齐到字;
  memset (common_grp_dsc.bg_reserved, 0, sizeof (common_grp_dsc.bg_reserved));
  //用NULL填充12个字;
}

//将某块/inode对应的字节设为1
void
set_block_used (_u8 *bitmap, int block_index)
{
  int byte_index = block_index / BITS_PER_BYTE; // 找到对应字节
  int bit_index = block_index % BITS_PER_BYTE;  // 找到字节中的位
  bitmap[byte_index] |= (1 << bit_index);       // 设置该位为1
}

//将某块/inode对应的字节设为0
void
set_block_free (_u8 *bitmap, int block_index)
{
  int byte_index = block_index / BITS_PER_BYTE; // 找到对应字节
  int bit_index = block_index % BITS_PER_BYTE;  // 找到字节中的位
  bitmap[byte_index] &= ~(1 << bit_index);      // 设置该位为0
}

//检查某一块/inode是否已用
int
is_block_used (_u8 *bitmap, int block_index)
{
  int byte_index = block_index / BITS_PER_BYTE;        // 找到对应字节
  int bit_index = block_index % BITS_PER_BYTE;         // 找到字节中的位
  return (bitmap[byte_index] & (1 << bit_index)) != 0; // 判断该位是否为1
}
//初始化一个bitmap
void
init_bitmap (_u8 *bitmap, int size)
{
  memset (bitmap, 0, size); // 全部置零
}

//为文件系统块设置对应的位图已用
void
bit_map_reserve_system_blocks (_u8 *bitmap, int count)
{
  for (int i = 0; i < count; i++)
    {
      set_block_used (bitmap, count);
    }
}

//创建common_blk_btmp
void
creat_common_blk_btmp ()
{
  init_bitmap (common_blk_btmp, sizeof (common_blk_btmp));
  bit_map_reserve_system_blocks (common_blk_btmp, 68);
}

//创建common_ind_btmp
void
creat_common_ind_btmp ()
{
  init_bitmap (common_ind_btmp, sizeof (common_ind_btmp));
}

// TODO:创建创建保留根目录inode的common_ind_btmp0;
void
creat_common_ind_btmp0 ()
{
}

// TODO:创建根目录
void
creat_root_dir ()
{
}

// TODO
void creat_file (){

};

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
  //打开fd
  init_fd ();
  //创建common_grp_dsc;
  creat_common_group_desc ();
  creat_common_blk_btmp ();
  creat_common_ind_btmp ();
  //按组写入img;
  for (int i = 0; i < group_number; i++)
    {
      creat_common_spr_blk (i);
      write_blocks (img_fd, &common_spr_blk, sizeof (common_spr_blk), 0, i);
      write_blocks (img_fd, &common_grp_dsc, sizeof (common_grp_dsc), 1, i);
      write_blocks (img_fd, &common_blk_btmp, sizeof (common_blk_btmp), 2, i);
      write_blocks (img_fd, &common_ind_btmp, sizeof (common_ind_btmp), 3, i);
    }
  // TODO
}

//分配空闲块
int
allocate_block (_u8 *bitmap, int total_blocks)
{
  for (int i = 0; i < total_blocks; i++)
    {
      if (!is_block_used (bitmap, i))
        {
          set_block_used (bitmap, i);
          return i; // 返回分配的块号
        }
    }
  return -1; // 无空闲块
  // TODO: 其他操作
}
//释放空闲块
void
free_block (_u8 *bitmap, int block_index)
{
  set_block_free (bitmap, block_index);
  // TODO：其他操作
}

void
parse_cmd ()
{
}

// int
// main ()
// {
//   init_file_system ();

//   return 0;
// }
