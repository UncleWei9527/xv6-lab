#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"

void
memdump(char *fmt, char *data, int len)
{
  //i: 四字节十进制数字
  //S: 把剩余数据当作个完整c字符串
  //s: 八字节当做指针，打印这个指针的字符串
  int i = 0;

  while (*fmt) {
    char *ptr = data + i;
    switch (*fmt) {
    case 'i': {
      //判断是否越界
      if (len - i < 4)
        goto Error;
      i += 4;
      int num = *(int *)ptr;
      printf("%d\n", num);
      break;
    }
    case 'p': {
      if (len - i < 8)
        goto Error;
      i += 8;
      unsigned long long num = *((unsigned long long *)ptr);
      printf("%llx\n", num);
      break;
    }
    case 'h': {
      if (len - i < 2)
        goto Error;
      i += 2;
      unsigned short num = *(unsigned short *)ptr;
      printf("%u\n", (unsigned int)num);
      break;
    }
    case 'c': {
      if (len - i < 1)
        goto Error;
      i += 1;
      char ch = *ptr;
      printf("%c\n", ch);
      break;
    }
    case 's': {
      if (len - i < 8)
        goto Error;
      i += 8;
      char *p = (char *)(*(unsigned long long *)ptr);
      printf("%s\n", p);
      break;
    }
    case 'S': {
      printf("%s\n", ptr);
      i = len;
      break;
    }
    default:
      fprintf(2, "memdump: unknown format:%c\n", *fmt);
      return;
    }

    fmt++;
    continue;
  Error:
    fprintf(2, "memdump: not enough data for \'%c\'", *fmt);
    exit(-1);
  }
  return;
}

int
main(int argc, char *argv[])
{
  if (argc == 1) {
    printf("Example 1:\n");
    int a[2] = {61810, 2026};
    memdump("ii", (char *)a, sizeof(a));

    printf("Example 2:\n");
    memdump("S", "a string", sizeof("a string"));

    printf("Example 3:\n");
    char *s = "another";
    memdump("s", (char *)&s, sizeof(s));

    struct sss {
      char *ptr;
      int num1;
      short num2;
      char byte;
      char bytes[8];
    } example;

    example.ptr = "hello";
    example.num1 = 1819438967;
    example.num2 = 100;
    example.byte = 'z';
    strcpy(example.bytes, "xyzzy");

    printf("Example 4:\n");
    memdump("pihcS", (char *)&example, sizeof(example));

    printf("Example 5:\n");
    memdump("sccccc", (char *)&example, sizeof(example));
  } else if (argc == 2) {
    // format in argv[1], up to 512 bytes of data from standard input.
    char data[512];
    int n = 0;
    memset(data, '\0', sizeof(data));
    while (n < sizeof(data)) {
      int nn = read(0, data + n, sizeof(data) - n);
      if (nn <= 0)
        break;
      n += nn;
    }
    memdump(argv[1], data, n);
  } else {
    printf("Usage: memdump [format]\n");
    exit(1);
  }
  exit(0);
}
