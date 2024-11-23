#include "include/common.h"

struct ext2_inode_info
{
  _u32 i_data[15];  //数据块指针数组;
  _u32 i_flags; //打开文件的方式;
  _u32 i_faddr; //片的地址;
  _u8 i_frage_no; //如果用到片,则是第一个片号;
  _u8 i_frag_size; //片大小;
  _u16 i_osync; //同步;
  _u32 i_file_acl;//文件访问控制链表;
  _u32 i_dir_acl;//目录访问控制链表
  _u32 i_dtime;//文件的删除时间;
  _u32 i_block_group; // 索引节点所在的块组号;
  // 一下四个变量用于操作预分配块
  _u32 i_next_alloc_block;
  _u32 i_next_alloc_goal;
  _u32 i_prealloc_block;
  _u32 i_prealloc_count;
  _u32 i_dir_start_lookup;
  int i_new_inode:1 ;//a freshly allocated inode;

};