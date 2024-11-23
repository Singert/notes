#include "include/common.h"
#define Ext2_MAX_GROUP_LOADED 64

//TODO:定义buffer_head,wait_queue

struct ext2_sb_info
{
  _u64 s_frag_size;          //片大小（以字节记）
  _u64 s_frags_per_block;    //每块中片数;
  _u64 s_inodes_per_block;   //每块中节点数;
  _u64 s_frags_per_group;    //每组中片数;
  _u64 s_blocks_per_group;   //每组中块数;
  _u64 s_inodes_per_group;   //每组中节点数;
  _u64 s_itb_per_group;      //每组中索引节点表所占的块数;
  _u64 s_db_per_group;       //每组中组描述符所占块数;
  _u64 s_desc_per_block;     //每块中组描述符数;
  _u64 s_groups_count;       //文件系统中块组数;
  struct buffer_head *s_sbh; //指向包含超级块的缓存;
  struct buffer_head *
      *s_group_desc; //指向高速缓存中组描述符表块的指针数组的一个指针;
  _u16 s_loaded_inode_bitmaps; //装入高速缓存的节点位图块数;
  _u16 s_loaded_block_bitmaps; //装入高速缓存的块位图数;
  _u64 s_inode_bitmap_number[Ext2_MAX_GROUP_LOADED];
  struct buffer_head *s_inode_bitmap[Ext2_MAX_GROUP_LOADED];
  _u64 s_block_bitmap_number[Ext2_MAX_GROUP_LOADED];
  struct buffer_head *s_block_bitmap[Ext2_MAX_GROUP_LOADED];
  int s_rename_lock; //重命名时的锁信号量;
  struct wait_queue * s_rename_wait;//指向重命名时的等待队列;
  _u64 s_mount_opt; //安装选项;
  _u16 s_resuid; //默认的用户标识号
  _u16 s_resgid; //默认的用户组标识号;
  _u16 s_mount_state; //专用于管理员的安装选项;
  _u16 s_pad; //填充;
  int s_inode_size;//节点的大小;
  int s_first_ino;//第一个节点号;
};