
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
// int
// main ()
// {
//   ext2_super_block_t spb;
//   // memset(spb.s_reserved,0,sizeof(spb.s_reserved));
//   printf ("%lu", sizeof (spb));
// }

// 写入到文件
// void
// write_to_file (const char *filename, const ext2_group_desc_t *desc)
// {
//   FILE *file = fopen (filename, "wb");
//   if (!file)
//     {
//       perror ("Failed to open file for writing");
//       return;
//     }
//   fwrite (desc, sizeof (ext2_group_desc_t), 1, file);
//   fclose (file);
// }
// void
// read_from_file (const char *filename, ext2_group_desc_t *desc)
// {
//   FILE *file = fopen (filename, "rb");
//   if (!file)
//     {
//       perror ("Failed to open file for reading");
//       return;
//     }
//   fread (desc, sizeof (ext2_group_desc_t), 1, file);
//   fclose (file);
// }

// int
// main ()
// {
//   ext2_group_desc_t common_grp_dsc;
//   common_grp_dsc.bg_block_bitmap = 2; //组中块位图所在的块号
//   common_grp_dsc.bg_inode_bitmap = 3; //族中索引节点位图所在的块号;
//   common_grp_dsc.bg_inode_table = 4;  //族中索引节点表的首块号;
//   common_grp_dsc.bg_free_blocks_count = 16316; //组中空闲块数;
//   common_grp_dsc.bg_free_inodes_count = 2048;  //组中空闲索引节点数;
//   common_grp_dsc.bg_used_dirs_count = 0; //组中分配给目录的节点数;
//                                          //填充对齐到字;
//   memset (common_grp_dsc.bg_reserved, 0, sizeof
//   (common_grp_dsc.bg_reserved));
//   // 写入到文件
//   write_to_file ("group_desc.img", &common_grp_dsc);

//   // 读取结构
//   ext2_group_desc_t read_desc;
//   read_from_file ("group_desc.img", &read_desc);
//   printf ("bg_block_bitmap: %u\n", read_desc.bg_block_bitmap);
//   printf ("bg_inode_bitmap: %u\n", read_desc.bg_inode_bitmap);
//   printf ("bg_inode_table: %u\n", read_desc.bg_inode_table);
//   printf ("bg_free_blocks_count: %u\n", read_desc.bg_free_blocks_count);
//   printf ("bg_free_inodes_count: %u\n", read_desc.bg_free_inodes_count);
//   printf ("bg_used_dirs_count: %u\n", read_desc.bg_used_dirs_count);

//   return 0;
// }
// #include "ext2_fs_initfs.c"

// int
// main ()
// {
//   init_file_system ();
//   return 0;
// }