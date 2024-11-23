#include "include/common.h"
#include <bits/types.h>
#include <unistd.h>

#define Ext2_N_BLOCKS                                                         \
  15 //块指针数组元素个数,12个直接块指针.一个一级间接块指针、二级间接块指针，三级间接块指针；

#include <sys/types.h>
struct ext2_super_block
//来描述 Ext2 文件系统整体信息的数据结构
{
  _u32 s_inodes_count;      //文件系统中索引节点总数；
  _u32 s_blocks_count;      //文件系统中中块数
  _u32 s_r_blocks_count;    //为超级用户保留的总块数；
  _u32 s_free_blocks_count; //文件系统中空闲块总数；
  _u32 s_free_inodes_count; //文件系统中空闲索引节点总数
  _u32 s_first_data_block;  // 文件系统中第一个数据块
  _u32 s_log_block_size;    //用于计算逻辑块大小；
  _u32 s_log_frag_size;     // 用于计算片大小；
  _u32 s_blocks_per_group;  //每组中块数；
  _u32 s_frags_per_group;   //每组中片数;
  _u32 s_inodes_per_group;  //每组中索引节点总数;
  _u32 s_mtime;             //最后一次安装操作的时间;
  _u32 s_wtime;             //最后一次对超级块进行写操作的时间;
  _u16 s_mnt_count;         //安装计数;
  _s16 s_max_mnt_count;     //最大可安装计数；
  _u16 s_magic;             //用于确定文件系统版本的标志;
  _u16 s_state;             //文件系统的状态;
  _u16 s_errors;            //当检测到错误时如何处理；
  _u16 s_minor_rev_level;   //次版本号；
  _u32 s_lastcheck;         //最后一次检测文件系统状态的时间；
  _u32 s_checkinterval; //两次对文件系统状态进行检测的间隔时间;
  _u32 s_rev_level;     //版本号;
  _u16 s_def_resuid;    //保留块的默认用户标识号；
  _u16 s_def_resgid;    //保留块的用户组标识号；

  // only for EXT2_DYNAMIC_REV superblocks only
  _u32 s_first_ino;              //第一个非保留的索引节点；
  _u16 s_inode_size;             //索引节点的大小；
  _u32 s_block_group_nr;         //该超级块的块组号；
  _u32 s_feature_compat;         //兼容特点的位图;
  _u32 s_feature_incompat;       //非兼容特点的位图;
  _u32 s_feature_ro_compat;      //只读兼容特点的位图；
  _u8 s_uuid[16];                // 128位的文件系统标志号；
  char s_volume_name[16];        //卷名；
  char s_last_mounted[64];       //最后一个安装点的路径名;
  _u32 s_algorithm_usage_bitmap; // 用于压缩；

  // performance hints.Directory preallocation should only
  //  happen if the EXT2_COMPAT_PREALLOC flag is on.
  _u8 s_prealloc_blocks;     //预分配的块数；
  _u8 s_prealloc_dir_blocks; //给目录预分配的块数；
  _u16 s_paddingl;
  _u32 s_reserved[204]; //用NULL填充块的末尾；
};

struct ext2_inode //索引节点
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
};


struct ext_group_desc
{
  _u32 bg_block_bitmap;//组中块位图所在的块号
  _u32 bg_inode_bitmap;//族中索引节点位图所在的块号;
  _u32 bg_inode_table;//族中索引节点表的首块号;
  _u16 bg_free_blocks_count;//组中空闲块数;
  _u16 bg_free_inodes_count;//组中空闲索引节点数;
  _u16 bg_used_dirs_count;//组中分配给目录的节点数;
  _u16 bg_pad; //填充对齐到字;
  _u32 bg_reserved[3]; //用NULL填充12个字;

};