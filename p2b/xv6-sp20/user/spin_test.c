#include "types.h"
#include "stat.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int fd, i;

  if(argc <= 1){
    exit();
  }

  fd = atoi(argv[1]) * 10000;

  for(i = 0; i < fd; )
  {
        i= i+1;
  }
  printf(1,"Done!!!!!!!!!!!!!! \n");
  exit();
}
