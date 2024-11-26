#include "include/common.h"
#include <bits/types.h>
#include <unistd.h>

#define BLK_SIZE 4096            //块大小为4KB
#define UNIVERSAL_GRP_BLKS 16384 //每个块组块数:16384
#define UNIVERSAL_GRP_SIZE                                                    \
  (BLK_SIZE * UNIVERSAL_GRP_BLKS) //每个块组的大小（byte）
#define IMG_PATH "../part3/files/test.img"

#define BLK_OFST BLK_SIZE                 //块偏移量
#define BLK_GRP_OFFSET UNIVERSAL_GRP_SIZE //块组偏移量
#define MIN_GRP_SIZE
#define PRE_ALLOC_FILE_BLOCKS 1 //预分配给文件的块数；
#define PRE_ALLOC_DIR_BLOCKS 1  //预分配给文件夹的块数
#define BITS_PER_BYTE 8

#define INODES_PER_GROUP 2048 //每个块组的索引节点个数

#define Ext2_N_BLOCKS                                                         \
  15 //块指针数组元素个数,12个直接块指针.一个一级间接块指针、二级间接块指针，三级间接块指针；

#include <sys/types.h>
typedef struct ext2_super_block_t //来描述 Ext2 文件系统整体信息的数据结构
{
  _u32 s_inodes_count;      //文件系统中索引节点总数；
  _u32 s_blocks_count;      //文件系统中中块数
  _u32 s_free_blocks_count; //文件系统中空闲块总数；
  _u32 s_free_inodes_count; //文件系统中空闲索引节点总数
  _u32 s_first_data_block;  // 文件系统中第一个数据块
  _u32 s_log_block_size;    //用于计算逻辑块大小；
  _u32 s_blocks_per_group;  //每组中块数；
  _u32 s_inodes_per_group;  //每组中索引节点总数;
  _u32 s_mtime;             //最后一次安装操作的时间;
  _u32 s_wtime;             //最后一次对超级块进行写操作的时间;

  // only for EXT2_DYNAMIC_REV superblocks only
  _u32 s_first_ino;      //第一个非保留的索引节点；
  _u16 s_inode_size;     //索引节点的大小；
  _u32 s_block_group_nr; //该超级块的块组号；

  _u8 s_prealloc_blocks;     //预分配的块数；
  _u8 s_prealloc_dir_blocks; //给目录预分配的块数；
} ext2_super_block_t;
//   common_spr_blk.s_inodes_count
//       = 2048 * group_number; // FIXME: 文件系统中索引节点总数；
//   common_spr_blk.s_blocks_count = 16384 * group_number; //文件系统中中块数
//   common_spr_blk.s_r_blocks_count; // FIXME:为超级用户保留的总块数；
//   common_spr_blk.s_free_blocks_count
//       = 16316 * group_number; //文件系统中空闲块总数；
//   common_spr_blk.s_free_inodes_count
//       = 2048 * group_number; //文件系统中空闲索引节点总数
//   common_spr_blk.s_first_data_block = 68; // 文件系统中第一个数据块的块号
//   common_spr_blk.s_log_block_size
//       = 3; //用于计算逻辑块大小（1--1024,2--2048 3--4096,4--8192；
//   common_spr_blk.s_blocks_per_group = 16384; //每组中块数；
//   common_spr_blk.s_inodes_per_group = 2048;  //每组中索引节点总数;
//   common_spr_blk.s_mtime = (_u32)time (NULL); //最后一次安装操作的时间;
//   common_spr_blk.s_wtime
//       = (_u32)time (NULL); //最后一次对超级块进行写操作的时间;
//   common_spr_blk.s_first_ino = 0;              //第一个非保留的索引节点；
//   common_spr_blk.s_inode_size = 128;             //索引节点的大小；
//   common_spr_blk.s_block_group_nr = 0;         //该超级块的块组号；

//   // performance hints.Directory preallocation should only
//   //  happen if the EXT2_COMPAT_PREALLOC flag is on.
//   common_spr_blk.s_prealloc_blocks = PRE_ALLOC_FILE_BLOCKS; //预分配的块数；
//   common_spr_blk.s_prealloc_dir_blocks
//       = PRE_ALLOC_DIR_BLOCKS; //给目录预分配的块数；
//   memset (common_spr_blk.s_reserved, 0,
//           sizeof (common_spr_blk.s_reserved)); //用NULL填充块的末尾；
// }

typedef struct ext2_inode_t //索引节点
{
  _u16 i_mode;        //文件类型和访问权限；
  _u16 i_uid;         //文件拥有者标识号；
  _u32 i_size;        //以字节计算的文件大小；
  _u32 i_atime;       //文件最后一次被访问的时间;
  _u32 i_ctime;       //该节点最后一次被修改的时间；
  _u32 i_mtime;       //文件内容最后一次被修改的时间；
  _u32 i_dtime;       //文件删除时间;
  _u16 i_gid;         //文件用户组标识符;
  _u16 i_links_count; //文件的硬链接计数;
  _u32 i_blocks;      //文件所占的块数;
  _u32 i_flags;       //打开文件的方式;
  union
  {
    struct
    {
      _u32 l_i_reserved1; // 用于 Linux 特定的保留字段
    } linux1;

    struct
    {
      _u32 h_i_translator; // 用于 HURD 系统的特定字段
    } hurd1;

    struct
    {
      _u32 m_i_reserved1; // 其他操作系统的保留字段
    } masix1;
  }; /*特定操作系统的信息*/ // TODO:gpt生成,须更改
  /*由于是
  union，在同一时间只能使用其中一个字段。具体使用取决于文件系统运行的操作系统类型。

  例如，在 Linux 上使用 ext2 文件系统时，可能访问如下：

  c
  复制代码
  inode.union_data.linux1.l_i_reserved1 = 0x12345678;
  在 HURD 上使用时：

  c
  复制代码
  inode.union_data.hurd1.h_i_translator = 0x87654321;
  */
  _u32 i_block[Ext2_N_BLOCKS]; //指向数据块的指针数组;
  _u32 i_version;              //文件的版本号,用于NFS;
  _u32 i_file_acl;             //文件访问控制表;
  _u32 i_dir_acl;              //目录访问控制表;
  _u8 l_i_frg;                 //每块中的片数;
  _u32 i_faddr;                //片的地址
  _u32 i_reserved[2];
} ext2_inode_t;

typedef struct ext2_group_desc_t
{
  _u32 bg_block_bitmap;      //组中块位图所在的块号
  _u32 bg_inode_bitmap;      //族中索引节点位图所在的块号;
  _u32 bg_inode_table;       //族中索引节点表的首块号;
  _u16 bg_free_blocks_count; //组中空闲块数;
  _u16 bg_free_inodes_count; //组中空闲索引节点数;
  _u16 bg_used_dirs_count;   //组中分配给目录的节点数;
  _u32 bg_reserved[3];       //用NULL填充12个字;
} ext2_group_desc_t;

typedef _u8 block_bit_map_t[UNIVERSAL_GRP_BLKS / BITS_PER_BYTE];
typedef _u8 inode_bit_map_t[INODES_PER_GROUP / BITS_PER_BYTE];