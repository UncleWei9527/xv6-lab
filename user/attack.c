#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"
int
my_strncmp(const char *p, const char *q, uint n)
{
  while (n > 0 && *p && *p == *q)
    n--, p++, q++;
  if (n == 0)
    return 0;
  return (uchar)*p - (uchar)*q;
}

#define DATASIZE (28*4096)
int
main(int argc, char *argv[])
{
  // Your code here.
  char *data = sbrk(DATASIZE);

  // avoid putting the secret near the very start of a page.
  // unsigned long align = ((unsigned long) data) % 4096;
  // if(align != 0)
  //   data += (4096 - align);
  //
  // but put it at a random-ish offset.
  // if(strlen(argv[1]) > 0)
  //   data += 32 + (unsigned int)(argv[1][0] & 0xff);

#define MSG "Here it is: "
  // MSG + data 
  int msg_len=strlen(MSG);
  for(int i=0;i<DATASIZE-msg_len;i++)
  {
    if(my_strncmp(data+i,MSG,msg_len)==0)
    {
      if(data[i+msg_len]!='\0')
      printf("%s\n",data+i+msg_len);
    }
  }
  //strcpy(data, MSG);
  //strcpy(data + strlen(MSG), argv[1]);


  exit(1);
}
