#include "ext2_fs.h"
#include <stdio.h>

int
main ()
{
  ext2_super_block_t spb;
  spb.s_reserved = NULL;
  printf ("%lu", sizeof (spb));
}