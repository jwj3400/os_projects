#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  printf(1,"priority = %d\n",getnice((int)argv[1]));
  printf(1,"changed priority = %d\n",setnice((int)argv[1], (int)argv[2]));
  exit();
}
