#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char *argv[])
{
  if (argc <= 1) {
    fprintf(2,"sleep: argument need more than 2 arguments\n");
    exit(-1);

  }
  uint32 num=atoi(argv[1]);
  pause(num);
  return 0;

  }


