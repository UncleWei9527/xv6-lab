#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include"kernel/fcntl.h"
int is_seperate_character(char ch)
{
  return strchr("-\r\t\n./ ",ch)!=0;
}
enum {
  Normal,
  Number
};
void ouput_num(int num)
{
  if(num%5==0||num%6==0)
  {
    printf("%d\n",num);
  }
}
void sixfive(int fd)
{
    char ch;
    int num=0;
    int state=Normal;
    while(read(fd,&ch,1)>0)
    {
      if(is_seperate_character(ch))
      {
        switch(state)
        {
          case Number:
          ouput_num(num);
            state=Normal;
            break;
        }
      }
      else 
      {
        switch(state)
        {
          case Number:
            num=num*10+ch-'0';
            break;
          case Normal:
            num=ch-'0';
            state=Number;
            break;
        }
      }
    }
    if(state==Number)
      ouput_num(num);

}
int 
main(int argc, char *argv[])
{
  if(argc<2)
  {
    sixfive(0);
    return 0;
  }
  for(int i=1;i<argc;i++)
  {
    int fd=open(argv[i],O_RDONLY);
    if(fd<0)
    {
      fprintf(2,"sixfive: cannot open %s\n",argv[i]);
      continue;
    }
    sixfive(fd);
    close(fd);
  }
  return 0;

}
