#include "ext2_fs_sb.h"
#include "ext2_fs.h"
#include "ext2_fs_i.h"
#include "include/common.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define blk_size 4096//块大小为4KB
#define universal_grp_blks //每个块组块数:16384


#define blk_ofst blk_size //块偏移量
#define blk_grp_ofst (blk_size*blk_grp_ofst)//块组偏移量

struct ext2_super_block_t common_spr_blk;
struct ext2_inode_t common_ind;

void
init_spr_blk ()
{
}

void
init_blk_grp ()
{
}
